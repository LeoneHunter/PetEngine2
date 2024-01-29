#pragma once
#include <functional>

#include "Runtime/Core/Core.h"
#include "Runtime/System/Renderer/UIRenderer.h"
#include "Style.h"

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
	class LayoutWidget;
	class Theme;

	template<typename T>
	concept WidgetSubclass = std::derived_from<T, Widget>;

	// User provided function used to spawn other widgets
	// Used in a tooltip and dragdrop 
	using SpawnerFunction = std::function<LayoutWidget*()>;

	// Visitor function that is being called on every widget in the tree 
	// when provided to the Widget::Visit() method
	using ConstWidgetVisitor = std::function<bool(const Widget*)>;
	using WidgetVisitor = std::function<bool(Widget*)>;

	enum class Axis { X, Y, Both };
	constexpr AxisIndex ToAxisIndex(Axis inAxis) { assert(inAxis != Axis::Both); return inAxis == Axis::X ? AxisX : AxisY; }
	constexpr Axis ToAxis(int inAxisIndex) { assert(inAxisIndex == 0 || inAxisIndex == 1); return inAxisIndex ? Axis::Y : Axis::X; }

	// Represent invalid point. I.e. null object for poins
	constexpr auto NOPOINT = Point(std::numeric_limits<float>::max());

	enum class EventCategory {
		Debug	= 0, // DebugLog
		System	= 1, // Draw, MouseMove, MouseButton, KeyboardKey
		Layout	= 2, // HitTest, ParentLayout, ChildLayout, Hover
	};

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
		virtual EventCategory GetCategory() const = 0;
	};

	/*
	* Request to draw a widget debug data
	*/
	class DebugLogEvent: public IEvent {
		DEFINE_CLASS_META(DebugLogEvent, IEvent)
	public:
		bool IsBroadcast() const override { return true; };
		EventCategory GetCategory() const override { return EventCategory::Debug; }
		Debug::PropertyArchive* Archive = nullptr;
	};

	/*
	* Event created when mouse button is being hold and mouse is moving
	*/
	class MouseDragEvent: public IEvent {
		DEFINE_CLASS_META(MouseDragEvent, IEvent)
	public:
		EventCategory GetCategory() const override { return EventCategory::System; }

		// If during this event mouse button is being held
		// This is the point of initial mouse press
		Point			InitPosLocal;
		Point			PosLocal;
		float2			Delta;
	};

	/*
	* Passed to children of a widget when it's layout needs update
	* Widgets that are not subclasses of LayoutWidget do not care about these events
	*/
	class ParentLayoutEvent: public IEvent {
		DEFINE_CLASS_META(ParentLayoutEvent, IEvent)
	public:

		ParentLayoutEvent()
			: Parent(nullptr)
			, bAxisChanged(true, true)
			, bForceExpand(false, false)
		{}

		ParentLayoutEvent(LayoutWidget*	inParent,
						float2			inConstraints, 
						Vec2<bool>		inAxisChanged = {true, true}, 
						Vec2<bool>		inForceExpand = {false, false})
			: Parent(inParent)
			, Constraints(inConstraints)
			, bAxisChanged(inAxisChanged)
			, bForceExpand(inForceExpand)
		{}

		EventCategory GetCategory() const override { return EventCategory::Layout; }

		// A compenent will be 0 if not changed
		LayoutWidget*	Parent;
		float2			Constraints;
		Vec2<bool>		bAxisChanged;
		Vec2<bool>		bForceExpand;
	};

	/*
	* Passed to the parent by a LayoutWidget widget
	* when its added, modified or deleted to update parent's and itself's layout
	* Widgets that are not subclasses of LayoutWidget do not care about these events
	*/
	class ChildLayoutEvent: public IEvent {
		DEFINE_CLASS_META(ChildLayoutEvent, IEvent)
	public:

		// What child change spawned the event
		enum Subtype {
			OnAdded,
			OnRemoved,
			OnChanged,
			OnVisibility
		};

		ChildLayoutEvent()
			: Subtype(OnChanged)
			, bAxisChanged(true, true)
			, Child(nullptr)
		{}

		ChildLayoutEvent(LayoutWidget* inChild,
				   float2 inSize, 
				   Vec2<bool> inAxisChanged = {true, true}, 
				   Subtype inType = OnChanged)
			: Subtype(inType)
			, bAxisChanged(inAxisChanged)
			, Size(inSize)
			, Child(inChild)
		{}

		EventCategory GetCategory() const override { return EventCategory::Layout; }

		Subtype			Subtype;
		Vec2<bool>		bAxisChanged;
		float2			Size;
		LayoutWidget*	Child;
	};




	
	/*
	* Stores a stack of widgets that is being hovered by the mouse
	* It grows from the root widget to the leaves
	* Usually leaves are buttons, text, icon
	* Top element is the last widget that was hit
	*/
	class HitStack {
	public:

		struct HitData {
			LayoutWidget*	Widget = nullptr;
			Point			HitPosLocal;
		};

	public:

		constexpr void Push(LayoutWidget* inWidget, Point inPosLocal) {
			Stack.push_back(HitData{inWidget, inPosLocal});
		}

		constexpr bool Empty() const { return Stack.empty(); }

		constexpr HitData& Top() { return Stack.back(); }
		
		// Find hit data for a specified widget
		constexpr std::optional<HitData> Find(const LayoutWidget* inWidget) const {
			for(const auto& hitData : Stack) {
				if(hitData.Widget == inWidget) return {hitData};
			}
			return {};
		}

		constexpr auto begin() { return Stack.rbegin(); }
		constexpr auto end() { return Stack.rend(); }

	private:
		std::vector<HitData> Stack;
	};

	/*
	* Called by the framework when mouse cursor is moving
	*/
	class HitTestEvent: public IEvent {
		DEFINE_CLASS_META(HitTestEvent, IEvent)
	public:

		// inInternalPos is a position inside the widget rect relative to origin
		constexpr void PushItem(LayoutWidget* inWidget, Point inInternalPos) {
			HitStack->Push(inWidget, inInternalPos);
		}

		constexpr Point GetLastHitPos() const {
			return HitStack->Empty() ? HitPosGlobal : HitStack->Top().HitPosLocal;
		}

		EventCategory GetCategory() const override { return EventCategory::Layout; }

	public:
		Point		HitPosGlobal;
		HitStack*	HitStack = nullptr;
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
		EventCategory GetCategory() const override { return EventCategory::Layout; }
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

		EventCategory GetCategory() const override { return EventCategory::System; }

		MouseButtonEnum			Button = MouseButtonEnum::None;
		bool					bButtonPressed = false;
		Point					MousePosGlobal;
		Point					MousePosLocal;
	};




	class DrawEvent : public IEvent {
		DEFINE_CLASS_META(DrawEvent, IEvent)
	public:

		bool IsBroadcast() const override { return true; };
		EventCategory GetCategory() const override { return EventCategory::System; }

		// Global coords of the parent origin
		// Each widget should update this before passing down
		Point		ParentOriginGlobal;
		DrawList*	DrawList = nullptr;
		Theme*		Theme = nullptr;
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

		Widget(const std::string& inID = {})
			: m_ID(inID)
			, m_Parent(nullptr)
			, m_Flags(WidgetFlags::None)
		{}

		virtual ~Widget() = default;

		void Destroy() {
			if(m_Parent) {
				m_Parent->Unparent(this);
			}
			/// TODO notify the system that we are deleted
		}
		
		// Calls a visitor callback on this object and children if bRecursive == true
		virtual bool VisitChildren(const ConstWidgetVisitor& inVisitor, bool bRecursive = true) const { return true; }

		// Visits parents in the tree recursively
		void VisitParent(const WidgetVisitor& inVisitor, bool bRecursive = true) {
			for(auto parent = m_Parent; parent; parent = parent->GetParent()) {
				const auto bContinue = inVisitor(parent);
				if(!bContinue || !bRecursive) return;
			}
		}
		
		/*
		* Called by a parent when widget is being parented
		* Here a widget can execute subclass specific parenting functionality
		* For example a LayoutWidget which has position and size can notify the parent that it needs to update it's layout
		* We cannot do it directly in the Parent() function because a LayoutWidget child can be parented later down the tree
		* and it can be wrapped in a few widgets which are not LayoutWidget subclasses
		*/
		virtual void OnParented(Widget* inParent) { 
			Assert(inParent);
			
			if(m_Parent && m_Parent != inParent) {
				m_Parent->Unparent(this);
			}
			m_Parent = inParent;
		};

		/*
		* Called by a parent when widget is being parented
		* Here a widget can execute subclass specific unparenting functionality. 
		*/
		virtual void OnUnparented() { m_Parent = nullptr; }

		// Could be called by a widget when it wants to be a child
		// Or could be called by a subclass to add a child
		virtual void Parent(Widget* inChild) { assert(false && "Calling 'Parent()' on a widget which cannot have children"); };
		virtual void Unparent(Widget* inChild) { assert(false && "Calling 'Unparent()' on a widget which cannot have children"); }

		virtual bool OnEvent(IEvent* inEvent) {	
			if(auto* event = inEvent->As<DebugLogEvent>()) {
				event->Archive->PushObject(GetClassName(), this, GetParent());
				DebugSerialize(*event->Archive);
			}
			return true;
		};

		virtual void DebugSerialize(Debug::PropertyArchive& inArchive) {
			inArchive.PushProperty("ID", m_ID);
			inArchive.PushProperty("WidgetFlags", m_Flags);
		}

		bool		 DispatchToParent(IEvent* inEvent) { if(m_Parent) { return m_Parent->OnEvent(inEvent); } return false; }		

	public:

		const std::string&	GetID() const { return m_ID; }

		Widget*				GetParent() { return m_Parent; }
		const Widget*		GetParent() const { return m_Parent; }

		// Gets nearest parent in the tree of the class T
		template<WidgetSubclass T>
		const T*	GetParent() const {
			for(auto* parent = GetParent(); parent; parent = parent->GetParent()) {
				if(parent->IsA<T>()) return static_cast<const T*>(parent);
			}
			return nullptr;
		}

		template<WidgetSubclass T>
		T* GetParent() { return const_cast<T*>(std::as_const(*this).GetParent<T>()); }

		template<WidgetSubclass T>
		const T* GetChild() const {
			const T* child = nullptr;

			VisitChildren([&](const Widget* inChild) {
				if(inChild->IsA<T>()) { child = static_cast<const T*>(inChild); return false; }
				return true;
			},
			true);

			return child;
		}

		template<WidgetSubclass T>
		T*		GetChild() { return const_cast<T*>(std::as_const(*this).GetChild<T>()); }

		bool	HasAnyFlags(WidgetFlags inFlags) const { return m_Flags & inFlags; }
		void	SetFlags(WidgetFlags inFlags) { m_Flags |= inFlags; }
		void	ClearFlags(WidgetFlags inFlags) { m_Flags &= ~inFlags; }

	private:

		// Optional user defined ID
		// Must be unique among siblings
		// Could be used to identify widgets from user code
		std::string m_ID;
		// Our parent widget who manages our lifetime and passes events
		Widget*		m_Parent;

		WidgetFlags	m_Flags;
	};





	/*
	* A widget that has no size and position and cannot be drawn
	* but can interact with other widgets and handle some events
	* Subclasses of this class: TabController, DragDropSource, DragDropTarget, PopupSpawner, TooltipSpawner
	*/
	class Controller: public Widget {
		DEFINE_CLASS_META(Controller, Widget)
	public:

		Controller(const std::string& inID = {})
			: Widget(inID)
			, m_Child(nullptr) 
		{}

		void Parent(Widget* inChild) override {
			Assert(inChild);
			if(m_Child.get() == inChild) return;
			m_Child.reset(inChild);
			m_Child->OnParented(this);
		}

		void Unparent(Widget* inChild) override {
			Assert(inChild);

			if(m_Child.get() == inChild) {
				m_Child.release();
				m_Child->OnUnparented();
			}
		}

		bool OnEvent(IEvent* inEvent) override {
			// ChildLayoutEvent goes up
			// DebugEvent goes in
			// Other events go down
			if(inEvent->IsA<ChildLayoutEvent>()) {
				return DispatchToParent(inEvent);

			} else if(inEvent->GetCategory() == EventCategory::Debug) {
				Super::OnEvent(inEvent);
			}
			return m_Child ? m_Child->OnEvent(inEvent) : false;
		};

		bool VisitChildren(const ConstWidgetVisitor& inVisitor, bool bRecursive = true) const override {
			if(!m_Child) return true;

			const auto bContinue = inVisitor(m_Child.get());
			if(!bContinue) return false;

			if(bRecursive) {
				const auto bContinue = m_Child->VisitChildren(inVisitor, bRecursive);
				if(!bContinue) return false;
			}
			return false;
		}

	private:
		std::unique_ptr<Widget> m_Child;
	};





	/*
	* A widget which have size and position
	* And possibly can be drawn
	*/
	class LayoutWidget: public Widget {
		DEFINE_CLASS_META(LayoutWidget, Widget)
	public:

		LayoutWidget(const std::string& inStyleClass, const LayoutStyle* inLayoutStyle, const std::string& inID = {})
			: Widget(inID) 
			, m_StyleClass(inStyleClass)
			, m_LayoutStyle(inLayoutStyle)
		{}

		void OnParented(Widget* inParent) override {
			Super::OnParented(inParent);
			auto parent = GetNearestLayoutParent();

			if(parent) {
				ChildLayoutEvent onChild;
				onChild.Child = this;
				onChild.Subtype = ChildLayoutEvent::OnAdded;
				parent->OnEvent(&onChild);
			}
		}

		void OnUnparented() override {
			auto parent = GetNearestLayoutParent();

			if(parent) {
				ChildLayoutEvent onChild;
				onChild.Child = this;
				onChild.Size = GetSize();
				onChild.Subtype = ChildLayoutEvent::OnRemoved;
				parent->OnEvent(&onChild);
			}
			Super::OnUnparented();
		}

		// Returns nearest parent which is LayoutWidget
		LayoutWidget* GetNearestLayoutParent() { return Super::GetParent<LayoutWidget>(); }

		AxisMode	GetAxisMode() const {
			return {
				HasAnyFlags(WidgetFlags::AxisXExpand) ? AxisMode::Expand : AxisMode::Shrink,
				HasAnyFlags(WidgetFlags::AxisYExpand) ? AxisMode::Expand : AxisMode::Shrink
			};
		}

		void	SetAxisMode(AxisMode inMode) {
			inMode.x == AxisMode::Expand ? SetFlags(WidgetFlags::AxisXExpand) : ClearFlags(WidgetFlags::AxisXExpand);
			inMode.y == AxisMode::Expand ? SetFlags(WidgetFlags::AxisYExpand) : ClearFlags(WidgetFlags::AxisYExpand);
		}

		void	SetAxisMode(u8 inAxisIdx, AxisMode::Mode inMode) {
			Assert(inAxisIdx < 2);
			if(inAxisIdx == AxisX) inMode == AxisMode::Mode::Expand ? SetFlags(WidgetFlags::AxisXExpand) : ClearFlags(WidgetFlags::AxisXExpand);
			else if(inAxisIdx == AxisY) inMode == AxisMode::Mode::Expand ? SetFlags(WidgetFlags::AxisYExpand) : ClearFlags(WidgetFlags::AxisYExpand);
		}

		// Hiddent objects won't draw themselves and won't handle hovering 
		// but layout update should be managed by the parent
		void	SetVisibility(bool bVisible) {
			bVisible ? ClearFlags(WidgetFlags::Hidden) : SetFlags(WidgetFlags::Hidden);
		}

		bool	IsVisible() const { return !HasAnyFlags(WidgetFlags::Hidden); }

		// Should be called by subclasses
		void	SetPos(Point inPos) { m_LocalPos = inPos; }
		void	SetSize(float2 inSize) { m_Size = inSize; }
		void	SetSize(Axis inAxis, float inSize) { inAxis == Axis::X ? m_Size.x = inSize : m_Size.y = inSize; }
		void	SetSize(bool inAxis, float inSize) { m_Size[inAxis] = inSize; }

		// Helper to set our size based on the size of content inside. 
		// Adds paddings to the content size. Used in a Button, Icon
		void	SetSizeFromInner(float2 inContentSize) { 
			m_Size.x = inContentSize.x + m_LayoutStyle->Paddings.Left + m_LayoutStyle->Paddings.Right;
			m_Size.y = inContentSize.y + m_LayoutStyle->Paddings.Top + m_LayoutStyle->Paddings.Bottom;
		}

		// Helper to set our size based on the constraints passed from the parent
		void	SetSizeFromOuter(float2 inConstraintsSize) {
			m_Size.x = inConstraintsSize.x - m_LayoutStyle->Margins.Left + m_LayoutStyle->Margins.Right;
			m_Size.y = inConstraintsSize.y - m_LayoutStyle->Margins.Top + m_LayoutStyle->Margins.Bottom;
		}
		void	SetSizeFromOuter(int inAxis, float inConstraintsSize) {
			inAxis == AxisX ?
				m_Size.x = inConstraintsSize - m_LayoutStyle->Margins.Left + m_LayoutStyle->Margins.Right :
				m_Size.y = inConstraintsSize - m_LayoutStyle->Margins.Top + m_LayoutStyle->Margins.Bottom;
		}

		Rect	GetRect() const { return {m_LocalPos, m_LocalPos + m_Size}; }
		float2	GetSize() const { return m_Size; }
		float	GetSize(bool bYAxis) const { return bYAxis ? m_Size.y : m_Size.x; }

		// Get main size minus paddings
		float2	GetInnerSize() const {
			return {m_Size.x - m_LayoutStyle->Paddings.Left + m_LayoutStyle->Paddings.Right,
					m_Size.y - m_LayoutStyle->Paddings.Top + m_LayoutStyle->Paddings.Bottom};
		}

		// Get main size plus margins
		float2  GetOuterSize() const {
			return {m_Size.x + m_LayoutStyle->Margins.Left + m_LayoutStyle->Margins.Right,
					m_Size.y + m_LayoutStyle->Margins.Top + m_LayoutStyle->Margins.Bottom};
		}

		Point	GetOriginLocal() const { return m_LocalPos; }

		const LayoutStyle* GetLayoutStyle() const { return m_LayoutStyle; }

		// Helper
		void	NotifyParentOnSizeChanged(int inAxis = 2) {
			auto parent = GetNearestLayoutParent();
			if(!parent) return;

			ChildLayoutEvent onChild;
			onChild.Child = this;
			onChild.Size = m_Size;
			onChild.Subtype = ChildLayoutEvent::OnChanged;

			if(inAxis == 2) {
				onChild.bAxisChanged = {true, true};
			} else {
				onChild.bAxisChanged[inAxis] = true;
			}
			parent->OnEvent(&onChild);
		}

		void	NotifyParentOnVisibilityChanged() {
			auto parent = GetNearestLayoutParent();
			if(!parent) return;

			ChildLayoutEvent onChild;
			onChild.Child = this;
			onChild.Size = m_Size;
			onChild.Subtype = ChildLayoutEvent::OnVisibility;
			parent->OnEvent(&onChild);
		}

		bool	OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->As<HitTestEvent>()) {
				if(!IsVisible()) return false;
				auto hitPosLocalSpace = event->GetLastHitPos();

				if(GetRect().Contains(hitPosLocalSpace)) {
					event->PushItem(this, hitPosLocalSpace - GetOriginLocal());
					return true;
				}

			} else if(auto* event = inEvent->As<ParentLayoutEvent>()) {
				// We will handle parent changes common to most widgets
				// Sources of this event are:
				//   - Parent size has changed
				//   - A sibling has been added
				//   - A sibling has been removed
				//   - A sibling has been modified and has changed the layout
				const auto prevSize = GetOuterSize();

				for(auto axis = 0; axis != 2; ++axis) {

					if(event->bAxisChanged[axis] && (GetAxisMode()[axis] == AxisMode::Expand || event->bForceExpand[axis])) {
						SetSizeFromOuter(axis, event->Constraints[axis]);
					}

					if(event->bForceExpand[axis]) {
						SetAxisMode(axis, AxisMode::Expand);
					}
				}
				return true;

			} else if(auto* event = inEvent->As<DebugLogEvent>()) {
				event->Archive->PushObject(GetClassName(), this, GetParent());
				DebugSerialize(*event->Archive);
			}
			return false;
		};

		void	DebugSerialize(Debug::PropertyArchive& inArchive) override {
			Super::DebugSerialize(inArchive);
			inArchive.PushProperty("StyleClass", m_StyleClass);
			inArchive.PushProperty("Origin", m_LocalPos);
			inArchive.PushProperty("Size", m_Size);
		}

	private:
		// Margins and paddings
		const LayoutStyle*	m_LayoutStyle;
		// Class selector for styles
		// Default is the c++ class name
		// But user can override it
		std::string			m_StyleClass;
		// Position in pixels relative to parent origin
		Point				m_LocalPos;
		// Our size updated during constraints resolution
		float2				m_Size;
	};





	/*
	* LayoutWidgett which contains children
	*/
	class Container: public LayoutWidget {
		DEFINE_CLASS_META(Container, LayoutWidget)
	public:

		Container(const std::string& inStyleClass, const LayoutStyle* inLayoutStyle, const std::string& inID = {})
			: LayoutWidget(inStyleClass, inLayoutStyle, inID)
		{}

		virtual bool DispatchToChildren(IEvent* inEvent) = 0;

		bool OnEvent(IEvent* inEvent) override {
			
			if(auto* event = inEvent->As<DrawEvent>()) {
				auto eventCopy = *event;
				eventCopy.ParentOriginGlobal += Super::GetOriginLocal();
				DispatchToChildren(&eventCopy);
				return true;

			} else if(inEvent->IsA<HitTestEvent>()) {
				// Dispatch HitTest only if we was hit
				auto bHandled = Super::OnEvent(inEvent);

				if(bHandled) {
					return DispatchToChildren(inEvent);
				}
				return false;

			} else if(inEvent->GetCategory() == EventCategory::Debug || inEvent->IsA<ParentLayoutEvent>()) {
				Super::OnEvent(inEvent);
				DispatchToChildren(inEvent);
				return true;
			}
			// Ignore ChildLayout. Should be handled by subclasses
			return false;
		}
	};



	/*
	* LayoutWidget container that has one child
	*/
	class SingleChildContainer: public Container {
		DEFINE_CLASS_META(SingleChildContainer, Container)
	public:

		SingleChildContainer(const std::string& inStyleClass, const LayoutStyle* inLayoutStyle, const std::string& inID = {})
			: Container(inStyleClass, inLayoutStyle, inID)
			, m_Child(nullptr) 
		{}

		// Called by a subclass or a widget which wants to be a child
		void Parent(Widget* inChild) override {
			Assert(inChild);
			if(m_Child.get() == inChild) return;
			m_Child.reset(inChild);
			m_Child->OnParented(this);

			const auto axisMode = Super::GetAxisMode();

			for(auto axis = 0; axis != 2; ++axis) {
				auto layoutChild = inChild->IsA<LayoutWidget>() ? inChild->As<LayoutWidget>() : inChild->GetChild<LayoutWidget>();

				if(layoutChild) {
					Assertm(axisMode[axis] == AxisMode::Expand || layoutChild->GetAxisMode()[axis] == AxisMode::Shrink,
							"Parent child axis shrink/expand mismatch. A child with an expanded axis has been added to the containter with that axis shrinked.");
				}
			}
		}

		void Unparent(Widget* inChild) override {
			Assert(inChild);

			if(m_Child.get() == inChild) {
				m_Child.release();
				m_Child->OnUnparented();
			}
		}

		bool DispatchToChildren(IEvent* inEvent) override {
			if(m_Child) {
				return m_Child->OnEvent(inEvent);
			}
			return false;
		}

		bool VisitChildren(const ConstWidgetVisitor& inVisitor, bool bRecursive = true) const override {
			if(!m_Child) return true;

			const auto bContinue = inVisitor(m_Child.get());
			if(!bContinue) return false;

			if(bRecursive) {
				const auto bContinue = m_Child->VisitChildren(inVisitor, bRecursive);
				if(!bContinue) return false;
			}
			return false;
		}

		bool OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->As<ChildLayoutEvent>()) {
				const auto bSizeChanged = OnChildLayout(event);

				if(bSizeChanged) {
					NotifyParentOnSizeChanged();
				}
				return true;
			}
			return Super::OnEvent(inEvent);
		}

	protected:

		// Helper to update our size based on AxisMode
		// @param bUpdateChildOnAdded if true, updates a newly added child
		// @return true if our size was changed by the child
		bool OnChildLayout(const ChildLayoutEvent* inEvent, bool bUpdateChildOnAdded = true) {
			const auto axisMode = GetAxisMode();
			const auto childAxisMode = inEvent->Child->GetAxisMode();
			bool bSizeChanged = false;

			if(inEvent->Subtype == ChildLayoutEvent::OnRemoved) {
				// Default behavior
				SetSize({0.f, 0.f});
				bSizeChanged = true;

			} else if(inEvent->Subtype == ChildLayoutEvent::OnChanged) {

				for(auto axis = 0; axis != 2; ++axis) {

					if(inEvent->bAxisChanged[axis] && axisMode[axis] == AxisMode::Shrink) {
						Assertf(childAxisMode[axis] == AxisMode::Shrink,
								"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked."
								"Child class and id: '{}' '{}', Parent class and id: '{}' '{}'", inEvent->Child->GetClassName(), inEvent->Child->GetID(), GetClassName(), GetID());

						SetSize(axis, inEvent->Size[axis]);
						bSizeChanged = true;
					}
				}

			} else if(inEvent->Subtype == ChildLayoutEvent::OnAdded) {
				bool bUpdateChild = false;

				for(auto axis = 0; axis != 2; ++axis) {

					if(axisMode[axis] == AxisMode::Shrink) {
						Assertf(childAxisMode[axis] == AxisMode::Shrink,
								"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked."
								"Child class and id: '{}' '{}', Parent class and id: '{}' '{}'", inEvent->Child->GetClassName(), inEvent->Child->GetID(), GetClassName(), GetID());

						SetSize(axis, inEvent->Size[axis]);
						bSizeChanged = true;

					} else if(childAxisMode[axis] == AxisMode::Expand) {
						bUpdateChild = true;
					}
				}

				// If child is expanded, it depends on us so update
				if(bUpdateChild && bUpdateChildOnAdded) {
					ParentLayoutEvent onParent;
					onParent.bAxisChanged = {true, true};
					onParent.Constraints = GetSize();
					inEvent->Child->OnEvent(&onParent);
				}
			}
			return bSizeChanged;
		}

	private:
		std::unique_ptr<Widget> m_Child;
	};



	/*
	* LayoutWidget container that has multiple children
	*/
	class MultiChildContainer: public Container {
		DEFINE_CLASS_META(MultiChildContainer, Container)
	public:

		MultiChildContainer(const std::string& inStyleClass, const LayoutStyle* inLayoutStyle, const std::string& inID = {})
			: Container(inStyleClass, inLayoutStyle, inID) 
		{}

		void Parent(Widget* inChild) override {
			Assert(inChild);
			auto& newChild = m_Children.emplace_back(inChild);
			newChild->OnParented(this);
		}

		void Unparent(Widget* inChild) override {
			Assert(inChild);

			auto it = std::ranges::find_if(m_Children, [&](const auto& Child) { return Child.get() == inChild; });
			Assert(it != m_Children.end());
			it->release();
			m_Children.erase(it);
			inChild->OnUnparented();
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

		void GetVisibleChildren(std::vector<LayoutWidget*>* outVisibleChildren) {
			outVisibleChildren->reserve(m_Children.size());

			for(auto& child : m_Children) {
				auto layoutChild = child->As<LayoutWidget>();

				if(!layoutChild) {
					layoutChild = child->GetChild<LayoutWidget>();
				}

				if(layoutChild && layoutChild->IsVisible()) {
					outVisibleChildren->push_back(layoutChild);
				}
			}
		}

		bool VisitChildren(const ConstWidgetVisitor& inVisitor, bool bRecursive = true) const override {

			for(auto& child : m_Children) {
				const auto bContinue = inVisitor(child.get());
				if(!bContinue) return false;

				if(bRecursive) {
					const auto bContinue = child->VisitChildren(inVisitor, bRecursive);
					if(!bContinue) return false;
				}
			}
			return true;
		}

		bool OnEvent(IEvent* inEvent) override {
			Assertf(!inEvent->IsA<ChildLayoutEvent>(), "ChildLayoutEvent is not handled by the MultiChildContainer subclass. Subclass is {}", GetClassName());
			return Super::OnEvent(inEvent);
		}

	private:
		std::vector<std::unique_ptr<Widget>> m_Children;
	};

}