#pragma once
#include <functional>

#include "Runtime/Core/Core.h"
#include "Runtime/System/Renderer/UIRenderer.h"

using Point = Vec2<float>;

namespace UI { class Widget; }

namespace Debug {

	/*
	 * Simple write only archive class
	 */
	class PropertyArchive {
	public:

		struct Object;

		using ObjectID = uintptr_t;

		using Visitor = std::function<bool(Object&)>;

		struct Property {

			Property(std::string_view inName, const std::string& inValue)
				: Name(inName)
				, Value(inValue) {}

			std::string_view	Name;
			std::string			Value;
		};

		struct Object {

			Object(std::string_view inClassName, ObjectID inObjectID, Object* inParent)
				: m_ClassName(inClassName)
				, m_ObjectID(inObjectID) 
				, m_Parent(inParent)
			{}

			void PushProperty(std::string_view inName, const std::string& inValue) {
				m_Properties.emplace_back(inName, inValue);
			}

			Object* EmplaceChild(std::string_view inClassName, ObjectID inObjectID) {
				return  &*m_Children.emplace_back(new Object(inClassName, inObjectID, this));
			}

			bool Visit(const Visitor& inVisitor) {
				const auto bContinue = inVisitor(*this);
				if(!bContinue) return false;

				for(auto& child : m_Children) { 
					const auto bContinue = child->Visit(inVisitor);
					if(!bContinue) return false;
				}
				return true;
			}

			ObjectID			m_ObjectID = 0;
			std::string_view	m_ClassName;
			std::list<Property> m_Properties;
			Object*				m_Parent = nullptr;
			std::list<std::unique_ptr<Object>>	m_Children;
		};

	public:

		// Visit objects recursively in depth first
		// Stops iteration if inVisitor returns false
		void VisitRecursively(const Visitor& inVisitor) {
			if(m_RootObjects.empty()) return;

			for(auto& child : m_RootObjects) { 
				const auto bContinue = child->Visit(inVisitor);
				if(!bContinue) return;
			}
		}

		void PushObject(std::string_view inClassName, UI::Widget* inWidget, UI::Widget* inParent) {

			if(!inParent || !m_CursorObject) {
				m_CursorObject = &*m_RootObjects.emplace_back(new Object(inClassName, (ObjectID)inWidget, nullptr));
				return;
			}
				
			if(m_CursorObject->m_ObjectID != (ObjectID)inParent) {

				for(Object* parent = m_CursorObject->m_Parent; parent; parent = parent->m_Parent) {

					if(parent->m_ObjectID == (ObjectID)inParent) {
						m_CursorObject = parent;
					}
				}
			}
			assert(m_CursorObject);
			m_CursorObject = m_CursorObject->EmplaceChild(inClassName, (ObjectID)inWidget);
		}

		// Push formatter property string
		void PushProperty(std::string_view inName, Point inProperty) {
			Assertm(m_CursorObject, "PushObject() should be called first");
			Assert(!inName.empty());
			m_CursorObject->PushProperty(inName, std::format("{:.0f}:{:.0f}", inProperty.x, inProperty.y));
		}

		void PushProperty(std::string_view inName, const std::string& inProperty) {
			Assertm(m_CursorObject, "PushObject() should be called first");
			Assert(!inName.empty());
			m_CursorObject->PushProperty(inName, inProperty);
		}

		template<typename Enum> requires std::is_enum_v<Enum>
		void PushProperty(std::string_view inName, Enum inProperty) {
			Assertm(m_CursorObject, "PushObject() should be called first");
			Assert(!inName.empty());
			m_CursorObject->PushProperty(inName, ToString(inProperty));
		}

		template<typename T> requires std::is_arithmetic_v<T>
		void PushProperty(std::string_view inName, T inProperty) {
			Assertm(m_CursorObject, "PushObject() should be called first");
			Assert(!inName.empty());

			if constexpr(std::is_integral_v<T>) {
				m_CursorObject->PushProperty(inName, std::format("{}", inProperty));
			} else {
				m_CursorObject->PushProperty(inName, std::format("{:.2f}", inProperty));
			}
		}

	public:
		std::list<std::unique_ptr<Object>>	m_RootObjects;
		Object*								m_CursorObject = nullptr;
	};

}

namespace UI {

	class Widget;
	class Style;

	template<typename T>
	concept WidgetSubclass = std::derived_from<T, Widget>;

	// User provided function used to spawn other widgets
	// Used in a tooltip and dragdrop 
	using SpawnerFunction = std::function<Widget* ()>;

	enum class Axis { X, Y, Both };
	constexpr AxisIndex ToAxisIndex(Axis inAxis) { assert(inAxis != Axis::Both); return inAxis == Axis::X ? AxisX : AxisY; }
	constexpr Axis ToAxis(int inAxisIndex) { assert(inAxisIndex == 0 || inAxisIndex == 1); return inAxisIndex ? Axis::Y : Axis::X; }


	/*
	* Interface to widget tree events
	* Events are executed immediatly
	*/
	class IEvent {
		DEFINE_ROOT_CLASS_META(IEvent)
	public:

		virtual void Log() {};
		virtual ~IEvent() = default;
		// Whether this event should be dispatched to all widgets
		// ignoring the return value
		// E.g. DrawEvent
		virtual bool IsBroadcast() const { return false; };
	};

	/*
	* Request to draw a widget debug data
	*/
	class DebugLogEvent: public IEvent {
		DEFINE_CLASS_META(DebugLogEvent, IEvent)
	public:
		bool IsBroadcast() const override { return true; };
		Debug::PropertyArchive* Archive = nullptr;
	};

	/*
	* Event created when mouse button is being hold and mouse is moving
	*/
	class MouseDragEvent: public IEvent {
		DEFINE_CLASS_META(MouseDragEvent, IEvent)
	public:

		// If during this event mouse button is being held
		// This is the point of initial mouse press
		Point	MousePressPosGlobal;
		Point	CursorPosGlobalSpace;
		Point	CursorPosLocalSpace;
		float2	CursorDelta;
		Widget* HoveredView;
	};

	/*
	* Passed to the children of a widget
	* when it's layout has been modified
	*/
	class ParentEvent: public IEvent {
		DEFINE_CLASS_META(ParentEvent, IEvent)
	public:

		ParentEvent()
			: bAxisChanged(true, true)
			, bForceExpand(false, false)
		{}

		ParentEvent(float2 inConstraints, 
					Vec2<bool> inAxisChanged = {true, true}, 
					Vec2<bool> inForceExpand = {false, false})
			: Constraints(inConstraints)
			, bAxisChanged(inAxisChanged)
			, bForceExpand(inForceExpand)
		{}

		// A compenent will be 0 if not changed
		float2		Constraints;
		Vec2<bool>	bAxisChanged;
		Vec2<bool>	bForceExpand;
	};

	/*
	* Passed to the parent by a child
	* when its added, modified or deleted
	*/
	class ChildEvent: public IEvent {
		DEFINE_CLASS_META(ChildEvent, IEvent)
	public:

		// What child change spawned the event
		enum Subtype {
			OnAdded,
			OnRemoved,
			OnChanged,
			OnVisibility
		};

		ChildEvent()
			: Subtype(OnChanged)
			, bAxisChanged(true, true)
			, Child(nullptr)
		{}

		ChildEvent(Widget* inChild, 
				   float2 inSize, 
				   Vec2<bool> inAxisChanged = {true, true}, 
				   Subtype inType = OnChanged)
			: Subtype(inType)
			, bAxisChanged(inAxisChanged)
			, Size(inSize)
			, Child(inChild)
		{}

		Subtype		Subtype;
		Vec2<bool>	bAxisChanged;
		float2		Size;
		Widget*		Child;
	};




	struct HitData {
		Widget* Widget = nullptr;
		Point	HitPosLocal;
	};

	/*
	* Called by the framework when mouse cursor is moving
	*/
	class HitTestEvent: public IEvent {
		DEFINE_CLASS_META(HitTestEvent, IEvent)
	public:

		// inInternalPos is a position inside the widget rect relative to origin
		void PushItem(Widget* inWidget, Point inInternalPos) {
			HitStack.push_back(HitData{inWidget, inInternalPos});
		}

		Point GetLastHitPos() const {
			return HitStack.empty() ? HitPosGlobal : HitStack.back().HitPosLocal;
		}

		Widget* GetLastWidget() {
			if(!HitStack.empty()) return HitStack.back().Widget;
			return nullptr;
		}

		std::optional<HitData> FindHitDataFor(const Widget* inWidget) const {
			for(const auto& hitData : HitStack) {
				if(hitData.Widget == inWidget) return {hitData};
			}
			return {};
		}

		void Clear() {
			HitPosGlobal = {0, 0};
			HitStack.clear();
		}

	public:
		Point					HitPosGlobal;
		std::vector<HitData>	HitStack;
	};




	/*
	* Dispatched to the last widget in the hittest stack
	* If the widget doen't want to handle the event it passes it to the parent
	* The dispatched also responsible for handling hover out events
	*/
	class HoverEvent: public IEvent {
		DEFINE_CLASS_META(HoverEvent, IEvent)
	public:
		HoverEvent(bool bHovered): bHovered(bHovered) {}
		bool bHovered;
	};



	enum class MouseButtonEnum {
		None,
		ButtonLeft,
		ButtonRight,
		ButtonMiddle,
		Button4,
		Button5
	};

	class MouseButtonEvent: public IEvent {
		DEFINE_CLASS_META(MouseButtonEvent, IEvent)
	public:

		MouseButtonEnum			Button;
		bool					bButtonPressed;
		Point					HitGlobalPos;
	};


	class DrawEvent : public IEvent {
		DEFINE_CLASS_META(DrawEvent, IEvent)
	public:

		bool IsBroadcast() const override { return true; };

		// Global coords of the parent origin
		// Each widget should update this before passing down
		Point		ParentOriginGlobal;
		DrawList*	DrawList = nullptr;
	};




	struct AxisMode {

		enum Mode {
			Expand, 
			Shrink
		};

		constexpr Mode operator[] (u8 inAxisIdx) const {
			assert(inAxisIdx < 2);
			if(inAxisIdx) return y;
			return x;
		}

		Mode x;
		Mode y;
	};

	constexpr auto AxisModeExpand = AxisMode(AxisMode::Expand, AxisMode::Expand);
	constexpr auto AxisModeShrink = AxisMode(AxisMode::Shrink, AxisMode::Shrink);

	enum class WidgetFlags {
		None		= 0x0,
		Hidden		= 0x01,
		AxisXExpand	= 0x02,
		AxisYExpand	= 0x04
	};
	DEFINE_ENUM_FLAGS_OPERATORS(WidgetFlags)	
	DEFINE_ENUMFLAGS_TOSTRING_4(WidgetFlags, None, Hidden, AxisXExpand, AxisYExpand)

	/*
	* Top object for all logical widgets
	* Represents only logic and layout parameters relative to parents
	* Each visible widget has a RenderObject associated with it which is used for drawing
	* Render object has absolute size and position in the OS window
	* Generally a Widget uses a CSS box model for visible representation
	*
	*	 _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
	*	|	     Margins define outer box
	*	|  *_ _ _ _ <- Here Border _ _ _ _
	*	|  |     Main box with paddings
	*	|  |	_ _ _ _ _ _ _ _ _ _ _ _ _
	*   |  |   |
	*	|  |   | Content box with chidren
	*	|  |   |	Or "Body"
	*   |  |   |_ _ _ _
	*
	*	* - Is our origin point defined by 'm_PositionLocal' relative to parent origin
	*
	*
	*	Widget update cycle:
	*		[Created]
	*		-> OnLayout()
	*		CreateRenderObject()
	*		Draw()
	*		ProcessEvents()
	*		[Becomes dirty] OR [Pending Delete]
	*		<- Go to OnLayout
	* 
	*
	*	4 cases of layout events:
	*			Expanded axis: depends on parent
	*			Shrinked axis: depends on content (children)
	*			Expanded overriden from parent: from Expanded widget
	*			Expanded overriden from children: from expanded children
	*
	*	To update us on Layout and Child events
	*			Update per axis:
	*				Handle overrides
	*				Change size
	*				Notify parent or children
	*
	*	Call virtual OnUpdateChildren after our size has been updated,
	*	so that children of FLexBox can be updated
	*
	*
	*
	* 
	*	TODO
	*		Handle children shrink until all minimal sizes are accomodated
	*
	*/
	class Widget {
		DEFINE_ROOT_CLASS_META(Widget)
	public:

		Widget(const std::string& inID = {}, AxisMode inDefaultAxisMode = AxisModeShrink)
			: m_ID(inID)
			, m_Parent(nullptr)
			, m_Flags(WidgetFlags::None)
		{
			SetAxisMode(inDefaultAxisMode);
			// Get our RenderObject from the system
			// Each render object stores a pointer to the style created by the user
			// or default
		}

		virtual ~Widget() = default;

		void Destroy() {
			if(m_Parent) {
				m_Parent->Unparent(this);
			}
			/// TODO notify the system that we are deleted
		}
		
		// Calls a visitor callback on this object and children if bRecursive == true
		//virtual bool VisitChildren(const std::function<void(Widget*)>& inVisitor, bool bRecursive = true) = 0;

		// Visits parents in the tree recursively
		void VisitParent(const std::function<bool(Widget*)>& inVisitor, bool bRecursive = true) {
			for(auto parent = m_Parent; parent; parent = parent->GetParent()) {
				const auto bContinue = inVisitor(parent);
				if(!bContinue || !bRecursive) return;
			}
		}
		
		// Called by parent when added as a child
		virtual void OnParented(Widget* inParent) { 
			Assert(inParent);
			
			if(m_Parent && m_Parent != inParent) {
				m_Parent->Unparent(this);
			}
			m_Parent = inParent;
		};

		// Could be called by a widget when it wants to be a child
		// Or could be called by a subclass to add a child
		virtual void Parent(Widget* inChild) { assert(false && "Calling 'Parent()' on a widget which cannot have children"); };
		virtual void Unparent(Widget* inChild) { assert(false && "Calling 'Unparent()' on a widget which cannot have children"); }

		virtual bool OnEvent(IEvent* inEvent) {			
			if(auto* event = inEvent->As<DebugLogEvent>()) {
				event->Archive->PushObject(_GetClassName(), this, GetParent());
				DebugSerialize(*event->Archive);
			}
			return true;
		};

		virtual void DebugSerialize(Debug::PropertyArchive& inArchive) {
			inArchive.PushProperty("ID", m_ID);
			inArchive.PushProperty("Origin", m_LocalPos);
			inArchive.PushProperty("Size", m_Size);
			inArchive.PushProperty("WidgetFlags", m_Flags);
		}

		bool		DispatchToParent(IEvent* inEvent) { if(m_Parent) { return m_Parent->OnEvent(inEvent); } return false; }

		AxisMode	GetAxisMode() const {
			return {
				HasAnyFlags(WidgetFlags::AxisXExpand) ? AxisMode::Expand : AxisMode::Shrink,
				HasAnyFlags(WidgetFlags::AxisYExpand) ? AxisMode::Expand : AxisMode::Shrink
			}; 
		}

		void		SetAxisMode(AxisMode inMode) { 
			inMode.x == AxisMode::Expand ? SetFlags(WidgetFlags::AxisXExpand) : ClearFlags(WidgetFlags::AxisXExpand);
			inMode.y == AxisMode::Expand ? SetFlags(WidgetFlags::AxisYExpand) : ClearFlags(WidgetFlags::AxisYExpand);
		}

		void		SetAxisMode(u8 inAxisIdx, AxisMode::Mode inMode) {
			Assert(inAxisIdx < 2);
			if(inAxisIdx == AxisX) inMode == AxisMode::Mode::Expand ? SetFlags(WidgetFlags::AxisXExpand) : ClearFlags(WidgetFlags::AxisXExpand);
			else if(inAxisIdx == AxisY) inMode == AxisMode::Mode::Expand ? SetFlags(WidgetFlags::AxisYExpand) : ClearFlags(WidgetFlags::AxisYExpand);
		}

		// Hiddent objects won't draw themselves and won't handle hovering 
		// but layout update should be managed by the parent
		void		SetVisibility(bool bVisible) {
			bVisible ? ClearFlags(WidgetFlags::Hidden) : SetFlags(WidgetFlags::Hidden);
		}

	public:

		const std::string&	GetID() const { return m_ID; }

		Widget*				GetParent() { return m_Parent; }
		const Widget*		GetParent() const { return m_Parent; }

		// Gets nearest parent in the tree of the class T
		template<WidgetSubclass T>
		Widget*	GetParentOf() { 
			for(auto* parent = GetParent(); parent; parent = parent->GetParent()) {
				if(parent->IsSubclassOf<T>) return parent;
			}
			return nullptr;
		}

		// Should be called by subclasses
		void	SetPos(Point inPos) { m_LocalPos = inPos; }
		void	SetSize(float2 inSize) { m_Size = inSize; }
		void	SetSize(Axis inAxis, float inSize) { inAxis == Axis::X ? m_Size.x = inSize : m_Size.y = inSize; }
		void	SetSize(bool inAxis, float inSize) { m_Size[inAxis] = inSize; }

		Rect	GetRect() const { return {m_LocalPos, m_LocalPos + m_Size}; }
		float2	GetSize() const { return m_Size; }
		float	GetSize(bool bYAxis) const { return bYAxis ? m_Size.y : m_Size.x; }
		Point	GetOriginLocal() const { return m_LocalPos; }

		bool	HasAnyFlags(WidgetFlags inFlags) const { return m_Flags & inFlags; }
		void	SetFlags(WidgetFlags inFlags) { m_Flags |= inFlags; }
		void	ClearFlags(WidgetFlags inFlags) { m_Flags &= ~inFlags; }

		bool	IsVisible() const { return !HasAnyFlags(WidgetFlags::Hidden); }

		// Helper
		void	NotifyParentOnSizeChanged(int inAxis = 2) {
			if(!m_Parent) return;
			
			ChildEvent onChild;
			onChild.Child = this;
			onChild.Size = m_Size;
			onChild.Subtype = ChildEvent::OnChanged;

			if(inAxis == 2) {
				onChild.bAxisChanged = {true, true};
			} else {
				onChild.bAxisChanged[inAxis] = true;
			}
			m_Parent->OnEvent(&onChild);
		}

		void	NotifyParentOnVisibilityChanged() {
			if(!m_Parent) return;

			ChildEvent onChild;
			onChild.Child = this;
			onChild.Size = m_Size;
			onChild.Subtype = ChildEvent::OnVisibility;
			m_Parent->OnEvent(&onChild);
		}

	private:

		// Optional user defined ID
		// Must be unique among siblings
		// Could be used to identify widgets from user code
		std::string m_ID;
		// Position in pixels relative to parent origin
		Point		m_LocalPos;
		// Our size updated during constraints resolution
		float2		m_Size;
		// Our parent widget who manages our lifetime and passes events
		Widget*		m_Parent;

		WidgetFlags	m_Flags;
	};





	/*
	* A widget which have size and position
	* And possibly can be drawn
	*/
	class LayoutWidget: public Widget {
		DEFINE_CLASS_META(LayoutWidget, Widget)
	public:

		LayoutWidget(const std::string& inID = {}): Widget(inID) {}

		bool OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->As<HitTestEvent>()) {
				if(!Super::IsVisible()) return false;
				auto hitPosLocalSpace = event->GetLastHitPos();

				if(GetRect().Contains(hitPosLocalSpace)) {
					event->PushItem(this, hitPosLocalSpace - GetOriginLocal());
					return true;
				}

			} else if(auto* event = inEvent->As<ParentEvent>()) {
				// We will handle parent changes common to most widgets
				// Sources of this event are:
				//   - Parent size has changed
				//   - A sibling has been added
				//   - A sibling has been removed
				//   - A sibling has been modified and has changed the layout
				const auto prevSize = GetSize();

				for(auto axis = 0; axis != 2; ++axis) {

					if(event->bAxisChanged[axis] && (GetAxisMode()[axis] == AxisMode::Expand || event->bForceExpand[axis])) {
						SetSize(axis, event->Constraints[axis]);
					}

					if(event->bForceExpand[axis]) {
						SetAxisMode(axis, AxisMode::Expand);
					}
				}
				// Let the subclass handle the children updates if needed
				return true;

			} else if(auto* event = inEvent->As<ChildEvent>()) {

				if(event->Subtype == ChildEvent::OnRemoved) {
					// Default behavior
					SetSize({0.f, 0.f});
					NotifyParentOnSizeChanged();
					return true;
				}
				const auto axisMode = GetAxisMode();
				const auto childAxisMode = event->Child->GetAxisMode();

				if(event->Subtype == ChildEvent::OnChanged) {

					for(auto axis = 0; axis != 2; ++axis) {

						if(event->bAxisChanged[axis] && axisMode[axis] == AxisMode::Shrink) {
							Assertf(childAxisMode[axis] == AxisMode::Shrink,
									"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked."
									"Child class and id: '{}' '{}', Parent class and id: '{}' '{}'", event->Child->_GetClassName(), event->Child->GetID(), _GetClassName(), GetID());

							SetSize(axis, event->Size[axis]);
							NotifyParentOnSizeChanged(axis);
						}
					}
					return true;
				}

				if(event->Subtype == ChildEvent::OnAdded) {

					for(auto axis = 0; axis != 2; ++axis) {

						if(axisMode[axis] == AxisMode::Shrink) {
							Assertf(childAxisMode[axis] == AxisMode::Shrink,
									"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked."
									"Child class and id: '{}' '{}', Parent class and id: '{}' '{}'", event->Child->_GetClassName(), event->Child->GetID(), _GetClassName(), GetID());

							SetSize(axis, event->Size[axis]);
							NotifyParentOnSizeChanged(axis);
						}
					}
				}

				ParentEvent onParent;
				onParent.bAxisChanged = {true, true};
				onParent.Constraints = Super::GetSize();
				event->Child->OnEvent(&onParent);

			} else if(auto* event = inEvent->As<DebugLogEvent>()) {
				event->Archive->PushObject(_GetClassName(), this, GetParent());
				DebugSerialize(*event->Archive);
			}
			return false;
		};
	};





	/*
	* A widget which contains children
	*/
	class Container: public LayoutWidget {
		DEFINE_CLASS_META(Container, LayoutWidget)
	public:

		Container(const std::string& inID = {}): LayoutWidget(inID) {}
		
		virtual bool DispatchToChildren(IEvent* inEvent) = 0;

		bool OnEvent(IEvent* inEvent) override {
			const auto bHandledBySuper = Super::OnEvent(inEvent);

			if(auto* event = inEvent->As<HitTestEvent>()) {

				if(bHandledBySuper) {
					DispatchToChildren(inEvent);
					return true;
				}

			} else if(auto* event = inEvent->As<DrawEvent>()) {
				// We can get here only if the event has been ignored by a subclass
				// So just dispatch draw to children
				auto eventCopy = *event;
				eventCopy.ParentOriginGlobal += Super::GetOriginLocal();
				DispatchToChildren(&eventCopy);
				return true;

			} else if(auto* event = inEvent->As<DebugLogEvent>()) {
				DispatchToChildren(event);
			}
			return bHandledBySuper;
		}
	};




	class SingleChildContainer: public Container {
		DEFINE_CLASS_META(SingleChildContainer, Container)
	public:

		SingleChildContainer(const std::string& inID = {})
			: Container(inID)
			, m_Child(nullptr) {}

		// Called by a subclass or a widget which wants to be a child
		void Parent(Widget* inChild) override {
			Assert(inChild);
			if(m_Child.get() == inChild) return;
			m_Child.reset(inChild);
			m_Child->OnParented(this);

			ChildEvent onChild;
			onChild.Child = inChild;
			onChild.Size = inChild->GetSize();
			onChild.Subtype = ChildEvent::OnAdded;
			this->OnEvent(&onChild);
		}

		void Unparent(Widget* inChild) override {
			Assert(inChild);

			if(m_Child.get() == inChild) {
				m_Child.release();

				ChildEvent onChild;
				onChild.Child = inChild;
				onChild.Size = inChild->GetSize();
				onChild.Subtype = ChildEvent::OnRemoved;
				this->OnEvent(&onChild);
			}
		}

		bool DispatchToChildren(IEvent* inEvent) override {
			if(m_Child) {
				return m_Child->OnEvent(inEvent);
			}
			return false;
		}

		Widget* GetChild() { return m_Child.get(); }

	private:
		std::unique_ptr<Widget> m_Child;
	};




	class MultiChildContainer: public Container {
		DEFINE_CLASS_META(MultiChildContainer, Container)
	public:

		MultiChildContainer(const std::string& inID = {})
			: Container(inID) {}

		void Parent(Widget* inChild) override {
			Assert(inChild);
			auto& newChild = m_Children.emplace_back(inChild);
			newChild->OnParented(this);

			ChildEvent onChild;
			onChild.Child = inChild;
			onChild.Size = inChild->GetSize();
			onChild.Subtype = ChildEvent::OnAdded;
			this->OnEvent(&onChild);
		}

		void Unparent(Widget* inChild) override {
			Assert(inChild);

			auto it = std::ranges::find_if(m_Children, [&](const auto& Child) { return Child.get() == inChild; });
			Assert(it != m_Children.end());
			it->release();
			m_Children.erase(it);

			ChildEvent onChild;
			onChild.Child = inChild;
			onChild.Size = inChild->GetSize();
			onChild.Subtype = ChildEvent::OnRemoved;
			this->OnEvent(&onChild);
		}

		bool DispatchToChildren(IEvent* inEvent) override {
			for(auto& child : m_Children) {
				auto result = child->OnEvent(inEvent);

				if(result && !inEvent->IsBroadcast()) {
					return result;
				}
			}
			return false;
		}

		void GetVisibleChildren(std::vector<Widget*>* outVisibleChildren) {
			outVisibleChildren->reserve(m_Children.size());

			for(auto& child : m_Children) {
				
				if(child->IsVisible()) {
					outVisibleChildren->push_back(&*child);
				}
			}
		}

	private:
		std::vector<std::unique_ptr<Widget>> m_Children;
	};

}