#pragma once
#include <functional>

#include "Runtime/Core/Core.h"
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
	class Tooltip;
	class TooltipSpawner;

	template<typename T>
	concept WidgetSubclass = std::derived_from<T, Widget>;

	// User provided function used to spawn other widgets
	// Used in a tooltip and dragdrop 
	using SpawnerFunction = std::function<LayoutWidget*()>;

	using Margin = RectSides<u32>;
	using Padding = RectSides<u32>;
	using ChildSlotIndex = size_t;

	enum class Axis { X, Y, Both };
	constexpr AxisIndex ToAxisIndex(Axis inAxis) { assert(inAxis != Axis::Both); return inAxis == Axis::X ? AxisX : AxisY; }
	constexpr Axis ToAxis(int inAxisIndex) { assert(inAxisIndex == 0 || inAxisIndex == 1); return inAxisIndex ? Axis::Y : Axis::X; }

	// Represent invalid point. I.e. null object for poins
	constexpr auto NOPOINT = Point(std::numeric_limits<float>::max());

	enum class EventCategory {
		Debug	= 0, // DebugLog
		System	= 1, // Draw, MouseMove, MouseButton, KeyboardKey
		Layout	= 2, // HitTest, ParentLayout, ChildLayout, Hover
		Intent	= 3, // SpawnPopup, SpawnDragDrop, SetClipboard, GetClipboard
	};

	// Mask to check pressed buttons
	enum class MouseButtonMask: u8 {
		None			= 0,
		ButtonLeft		= 0x1,
		ButtonRight		= 0x2,
		ButtonMiddle	= 0x4,
		Button4			= 0x8,
		Button5			= 0x10,
	};
	DEFINE_ENUM_FLAGS_OPERATORS(MouseButtonMask)

	enum class MouseButton: u8 {
		None,
		ButtonLeft,
		ButtonRight,
		ButtonMiddle,
		Button4,
		Button5,
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
	class DebugLogEvent final: public IEvent {
		DEFINE_CLASS_META(DebugLogEvent, IEvent)
	public:
		bool IsBroadcast() const override { return true; };
		EventCategory GetCategory() const override { return EventCategory::Debug; }
		Debug::PropertyArchive* Archive = nullptr;
	};

	/*
	* Only a top Window widget receives this event
	* Children widget receive HoverEvent and MouseDragEvent
	*/
	class MouseDragEvent final: public IEvent {
		DEFINE_CLASS_META(MouseDragEvent, IEvent)
	public:
		EventCategory GetCategory() const override { return EventCategory::System; }

		// If during this event mouse button is being held
		// This is the point of initial mouse press
		Point			MousePosOnCaptureLocal;
		// Position of the mouse inside the hovered widget
		// Relative to the widget size
		Point			MousePosOnCaptureInternal;
		Point			MousePosLocal;		
		float2			MouseDelta;
		MouseButtonMask	MouseButtonsPressedBitField = MouseButtonMask::None;
	};

	/*
	* Passed to children of a widget when it's layout needs update
	* Widgets that are not subclasses of LayoutWidget do not care about these events
	* Constraints define the area in which a child can position itself and change size
	*/
	class ParentLayoutEvent final: public IEvent {
		DEFINE_CLASS_META(ParentLayoutEvent, IEvent)
	public:

		ParentLayoutEvent()
			: Parent(nullptr)
		{}

		ParentLayoutEvent(LayoutWidget*	inParent, Rect inConstraints)
			: Parent(inParent)
			, Constraints(inConstraints)
		{}

		EventCategory	GetCategory() const override { return EventCategory::Layout; }

		LayoutWidget*	Parent;
		Rect			Constraints;
	};

	/*
	* Passed to the parent by a LayoutWidget widget
	* when its added, modified or deleted to update parent's and itself's layout
	* Widgets that are not subclasses of LayoutWidget do not care about these events
	*/
	class ChildLayoutEvent final: public IEvent {
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
		constexpr const HitData* Find(const LayoutWidget* inWidget) const {
			for(const auto& hitData : Stack) {
				if(hitData.Widget == inWidget) 
					return &hitData;
			}
			return nullptr;
		}

		constexpr auto begin() { return Stack.rbegin(); }
		constexpr auto end() { return Stack.rend(); }

	private:
		std::vector<HitData> Stack;
	};

	/*
	* Called by the framework when mouse cursor is moving
	*/
	class HitTestEvent final: public IEvent {
		DEFINE_CLASS_META(HitTestEvent, IEvent)
	public:

		// inInternalPos is a position inside the widget rect relative to origin
		constexpr void PushItem(LayoutWidget* inWidget, Point inInternalPos) {
			HitStack.Push(inWidget, inInternalPos);
		}

		constexpr Point GetLastHitPos() {
			return HitStack.Empty() ? HitPosGlobal : HitStack.Top().HitPosLocal;
		}

		EventCategory GetCategory() const override { return EventCategory::Layout; }

	public:
		Point		HitPosGlobal;
		HitStack	HitStack;
	};




	/*
	* Dispatched to the last widget in the hittest stack
	* If the widget doen't want to handle the event it passes it to the parent
	* The dispatched also responsible for handling hover out events
	*/
	class HoverEvent final: public IEvent {
		DEFINE_CLASS_META(HoverEvent, IEvent)
	public:
		HoverEvent(bool bHovered): bHovered(bHovered) {}
		EventCategory GetCategory() const override { return EventCategory::Layout; }
		bool bHovered;
	};



	

	class MouseButtonEvent final: public IEvent {
		DEFINE_CLASS_META(MouseButtonEvent, IEvent)
	public:

		EventCategory GetCategory() const override { return EventCategory::System; }

		MouseButton	Button = MouseButton::None;
		bool		bButtonPressed = false;
		Point		MousePosGlobal;
		Point		MousePosLocal;
	};




	class Drawlist {
	public:

		virtual ~Drawlist() = default;
		virtual void PushBox(Rect inRect, const BoxStyle* inStyle) = 0;
		virtual void PushBox(Rect inRect, Color inColor, bool bFilled = true) = 0;

		virtual void PushText(Point inOrigin, const TextStyle* inStyle, std::string_view inTextView) = 0;
		virtual void PushClipRect(Rect inClipRect) = 0;
	};

	class DrawEvent final: public IEvent {
		DEFINE_CLASS_META(DrawEvent, IEvent)
	public:

		bool IsBroadcast() const override { return true; };
		EventCategory GetCategory() const override { return EventCategory::System; }

		// Global coords of the parent origin
		// Each widget should update this before passing down
		Point		ParentOriginGlobal;
		Drawlist*	DrawList = nullptr;
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
	* A slot in which a widget is added
	* A widget can have multiple slots
	*/
	class WidgetAttachSlot {
	public:
		virtual ~WidgetAttachSlot() = default;

		virtual void Parent(Widget* inChild) = 0;
		virtual void Orphan(Widget* inChild) = 0;

		WidgetAttachSlot(Widget* inOwner)
			: m_Owner(inOwner) {}

	protected:
		Widget* m_Owner;
	};



	/*
	* Configuration data for a widget base class
	*/
	struct WidgetConfig {
		std::string			m_ID;
		std::string			m_StyleClass;
		TooltipSpawner*		m_Tooltip = nullptr;
		WidgetAttachSlot*	m_Slot = nullptr;
	};

	/*
	* Helper for easier widget creation through builders
	* T should be a subclass to allow chaining
	*/
	template<class T>
	struct WidgetBuilder: public WidgetConfig {

		T& ID(const std::string& inID) { m_ID = inID; return static_cast<T&>(*this); }
		T& StyleClass(const std::string& inStyleClass) { m_StyleClass = inStyleClass; return static_cast<T&>(*this); }
		T& Tooltip(TooltipSpawner* inTooltip) { m_Tooltip = inTooltip; return static_cast<T&>(*this); }

		constexpr operator WidgetConfig() const { return static_cast<WidgetConfig>(*this); }
	};


	// Returned by the callback to instruct iteration
	struct VisitResult {
		// If false stops iteration and exit from all visit calls
		bool bContinue = true;
		// Skip iterating children of a current widget
		bool bSkipChildren = false;		
	};
	constexpr auto VisitResultContinue = VisitResult(true, false);
	constexpr auto VisitResultExit = VisitResult(false, false);
	constexpr auto VisitResultSkipChildren = VisitResult(true, true);
	// Visitor function that is being called on every widget in the tree 
	// when provided to the Widget::Visit() method
	using WidgetVisitor = std::function<VisitResult(Widget*)>;


	/*
	* Top object for all logical widgets
	* Provides basic functionality for all widgets
	* Basic concept is that every widget in the tree has only limited functionality 
	* And the user can connect these widget as they like in any order
	* For example a PopupSpawner can wrap any layout widget and can provide a popup for it when hovered
	* Or a Centered widget which centers it's child inside parent's constraints
	*
	* There are two big classes of widgets: Controllers and LayoutWidgets
	*	  Controllers has only logic and can control and manage other widgets in the tree
	*	      Some controllers can take events from the user, like a popup controller which spawns a popup on left click
	*     LayoutWidgets has a visual representation and can be interacted by the user directly
	*/
	class Widget {
		DEFINE_ROOT_CLASS_META(Widget)
	public:

		Widget(const std::string& inID = {})
			: m_ID(inID)
			, m_Parent(nullptr)
			, m_Flags(WidgetFlags::None)
		{}

		virtual				~Widget() = default;

		void				Destroy() {
			/// TODO notify the system that we are deleted
		}
		
		// Calls a visitor callback on this object and children if bRecursive == true
		virtual VisitResult	VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive = true) { return VisitResultContinue; }

		// Visits parents in the tree recursively
		void				VisitParent(const WidgetVisitor& inVisitor, bool bRecursive = true) {
			for(auto parent = m_Parent; parent; parent = parent->GetParent()) {
				const auto result = inVisitor(parent);
				if(!result.bContinue || !bRecursive) return;
			}
		}

		// Attaches this widget to a parent slot
		void				Parent(WidgetAttachSlot& inSlot) { inSlot.Parent(this); }
		void				Orphan(WidgetAttachSlot& inSlot) { inSlot.Orphan(this); }
		
		/*
		* Called by a parent when widget is being parented
		* Here a widget can execute subclass specific parenting functionality
		* For example a LayoutWidget which has position and size can notify the parent that it needs to update it's layout
		* We cannot do it directly in the Parent() function because a LayoutWidget child can be parented later down the tree
		* and it can be wrapped in a few controller widgets
		*/
		virtual void		OnParented(Widget* inParent) { m_Parent = inParent; };

		/*
		* Called by a parent when widget has been orphaned from parent
		* Here a widget can execute subclass specific functionality.
		* Generally a widget is not deleted on this step
		*/
		virtual void		OnOrphaned() { m_Parent = nullptr; }

		virtual void		ParentChild(Widget* inChild, WidgetAttachSlot*) {}
		virtual void		OrphanChild(Widget* inChild) {}
			 
		virtual bool		OnEvent(IEvent* inEvent) {	
			if(auto* event = inEvent->Cast<DebugLogEvent>()) {
				event->Archive->PushObject(GetClassName(), this, GetParent());
				DebugSerialize(*event->Archive);
			}
			return true;
		};

		virtual void		DebugSerialize(Debug::PropertyArchive& inArchive) {
			inArchive.PushProperty("ID", m_ID);
			inArchive.PushProperty("WidgetFlags", m_Flags);
		}

		bool				DispatchToParent(IEvent* inEvent) { if(m_Parent) { return m_Parent->OnEvent(inEvent); } return false; }		

	public:

		const std::string&	GetID() const { return m_ID; }

		Widget*				GetParent() { return m_Parent; }
		const Widget*		GetParent() const { return m_Parent; }

		// Gets nearest parent in the tree of the class T
		template<WidgetSubclass T>
		const T*			GetParent() const {
			for(auto* parent = GetParent(); parent; parent = parent->GetParent()) {
				if(parent->IsA<T>()) 
					return static_cast<const T*>(parent);
			}
			return nullptr;
		}

		template<WidgetSubclass T>
		T*					GetParent() { return const_cast<T*>(std::as_const(*this).GetParent<T>()); }

		// Looks for a child with specified class
		// @param bRecursive - Looks among children of children
		template<WidgetSubclass T>
		T*					GetChild(bool bRecursive = true) {
			T* child = nullptr;

			VisitChildren([&](Widget* inChild) {
				if(inChild->IsA<T>()) {
					child = static_cast<T*>(inChild);
					return VisitResultExit; 
				}
				return VisitResultContinue;
			},
			bRecursive);

			return child;
		}

		bool				HasAnyFlags(WidgetFlags inFlags) const { return m_Flags & inFlags; }
		void				SetFlags(WidgetFlags inFlags) { m_Flags |= inFlags; }
		void				ClearFlags(WidgetFlags inFlags) { m_Flags &= ~inFlags; }

		// Returns a debug identifier of this object
		// [MyWidgetClassName: <4 bytes of adress> "MyObjectID"]
		std::string			GetDebugIDString() {

			if(!GetID().empty()) {
				return std::format("[{}: {:x} \"{}\"]", GetClassName(), (uintptr_t)this & 0xffff, GetID());
			} else {
				return std::format("[{}: {:x}]", GetClassName(), (uintptr_t)this & 0xffff);
			}
		}

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
	* Slot for a single child
	*/
	template<typename T = Widget>
	class SingleWidgetSlot: public WidgetAttachSlot {
	public:

		SingleWidgetSlot(Widget* inOwner)
			: WidgetAttachSlot(inOwner) {}

		void Parent(Widget* inChild) override {
			if(!inChild || inChild == m_Child.get()) return;
			m_Child.reset(inChild->Cast<T>());
			m_Child->OnParented(m_Owner);
		}

		void Orphan(Widget* inChild) override {
			m_Child.release();
			inChild->OnOrphaned();
		}

		operator bool() const { return m_Child != nullptr; }

		SingleWidgetSlot& operator=(Widget* inWidget) {
			Parent(inWidget);
			return *this;
		}

		T* operator->() { return m_Child.get(); }
		const T* operator->() const { return m_Child.get(); }

		T* Get() { return m_Child.get(); }

	public:
		std::unique_ptr<T> m_Child;
	};

	template<typename T>
	bool operator==(const SingleWidgetSlot<T>& inSlot, const Widget* inWidget) {
		return inSlot.Get() == inWidget;
	}



	class MultiChildSlot: public WidgetAttachSlot {
	public:

		MultiChildSlot(Widget* inOwner)
			: WidgetAttachSlot(inOwner) {}

		void Parent(Widget* inChild) override {
			Assert(inChild && this);
			m_Children.emplace_back(inChild);
			inChild->OnParented(m_Owner);
		}

		void Orphan(Widget* inChild) override {
			m_Children.erase(std::ranges::find_if(m_Children, 
				[&](const auto& inPtr) { return inPtr.get() == inChild; }));
			inChild->OnOrphaned();
		}

		auto& Get() { return m_Children; }

		size_t Size() const { return m_Children.size(); }

	public:
		std::vector<std::unique_ptr<Widget>> m_Children;
	};



	/*
	* A widget that has no size and position and cannot be drawn
	* but can interact with other widgets and handle some events
	* Subclasses of this class: TabController, DragDropSource, DragDropTarget, PopupSpawner, TooltipSpawner
	* When placed in the tree, transparent to most events
	*/
	class Controller: public Widget {
		DEFINE_CLASS_META(Controller, Widget)
	public:

		Controller(const std::string& inID = {})
			: Widget(inID)
			, ChildSlot(this) 
		{}

		bool OnEvent(IEvent* inEvent) override {
			// ChildLayoutEvent goes up
			// DebugEvent goes in
			// Other events go down
			if(inEvent->GetCategory() == EventCategory::Intent) {
				return DispatchToParent(inEvent);

			} else if(inEvent->IsA<ChildLayoutEvent>()) {
				return DispatchToParent(inEvent);

			} else if(inEvent->GetCategory() == EventCategory::Debug) {
				Super::OnEvent(inEvent);
			}
			return ChildSlot ? ChildSlot->OnEvent(inEvent) : false;
		};

		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive = true) final {
			if(!ChildSlot) return VisitResultContinue;

			const auto result = inVisitor(ChildSlot.Get());
			if(!result.bContinue) return VisitResultExit;

			if(bRecursive && !result.bSkipChildren) {
				const auto result = ChildSlot->VisitChildren(inVisitor, bRecursive);
				if(!result.bContinue) return {false};
			}
			return VisitResultContinue;
		}

	public:
		SingleWidgetSlot<> ChildSlot;
	};





	/*
	* A widget which have size and position
	* And possibly can be drawn
	*/
	class LayoutWidget: public Widget {
		DEFINE_CLASS_META(LayoutWidget, Widget)
	public:

		LayoutWidget(const std::string& inID = {}): Widget(inID) {}

		void			OnParented(Widget* inParent) override {
			Super::OnParented(inParent);
			auto parent = GetNearestLayoutParent();

			if(parent) {
				ChildLayoutEvent onChild;
				onChild.Child = this;
				onChild.Size = m_Size;
				onChild.Subtype = ChildLayoutEvent::OnAdded;
				parent->OnEvent(&onChild);
			}
		}

		void			OnOrphaned() override {
			auto parent = GetNearestLayoutParent();

			if(parent) {
				ChildLayoutEvent onChild;
				onChild.Child = this;
				onChild.Size = m_Size;
				onChild.Subtype = ChildLayoutEvent::OnRemoved;
				parent->OnEvent(&onChild);
			}
			Super::OnOrphaned();
		}

		bool			OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->Cast<HitTestEvent>()) {
				if(!IsVisible()) return false;
				auto hitPosLocalSpace = event->GetLastHitPos();

				if(GetRect().Contains(hitPosLocalSpace)) {
					event->PushItem(this, hitPosLocalSpace - GetOrigin());
					return true;
				}
				return false;

			} else if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
				ExpandToParent(event);
				return true;

			} else if(auto* event = inEvent->Cast<DebugLogEvent>()) {
				event->Archive->PushObject(GetClassName(), this, GetParent());
				DebugSerialize(*event->Archive);

			} else if(inEvent->GetCategory() == EventCategory::Intent) {
				return DispatchToParent(inEvent);
			}
			return false;
		};

		void			DebugSerialize(Debug::PropertyArchive& inArchive) override {
			Super::DebugSerialize(inArchive);
			inArchive.PushProperty("Origin", m_Origin);
			inArchive.PushProperty("Size", m_Size);
		}

	public:

		// Returns nearest parent which is LayoutWidget
		LayoutWidget*	GetNearestLayoutParent() { return Super::GetParent<LayoutWidget>(); }

		AxisMode		GetAxisMode() const {
			return {
				HasAnyFlags(WidgetFlags::AxisXExpand) ? AxisMode::Expand : AxisMode::Shrink,
				HasAnyFlags(WidgetFlags::AxisYExpand) ? AxisMode::Expand : AxisMode::Shrink
			};
		}

		void			SetAxisMode(AxisMode inMode) {
			inMode.x == AxisMode::Expand ? SetFlags(WidgetFlags::AxisXExpand) : ClearFlags(WidgetFlags::AxisXExpand);
			inMode.y == AxisMode::Expand ? SetFlags(WidgetFlags::AxisYExpand) : ClearFlags(WidgetFlags::AxisYExpand);
		}

		void			SetAxisMode(u8 inAxisIdx, AxisMode::Mode inMode) {
			Assert(inAxisIdx < 2);
			if(inAxisIdx == AxisX) inMode == AxisMode::Mode::Expand ? SetFlags(WidgetFlags::AxisXExpand) : ClearFlags(WidgetFlags::AxisXExpand);
			else if(inAxisIdx == AxisY) inMode == AxisMode::Mode::Expand ? SetFlags(WidgetFlags::AxisYExpand) : ClearFlags(WidgetFlags::AxisYExpand);
		}

		// Hiddent objects won't draw themselves and won't handle hovering 
		// but layout update should be managed by the parent
		void			SetVisibility(bool bVisible) {
			bVisible ? ClearFlags(WidgetFlags::Hidden) : SetFlags(WidgetFlags::Hidden);
		}

		bool			IsVisible() const { return !HasAnyFlags(WidgetFlags::Hidden); }

		// Should be called by subclasses
		void			SetOrigin(Point inPos) { m_Origin = inPos; }
		void			SetOrigin(int Axis, float inPos) { m_Origin[Axis] = inPos; }

		void			SetSize(float2 inSize) { m_Size = inSize; }
		void			SetSize(Axis inAxis, float inSize) { inAxis == Axis::X ? m_Size.x = inSize : m_Size.y = inSize; }
		void			SetSize(bool inAxis, float inSize) { m_Size[inAxis] = inSize; }

		Rect			GetRect() const { return {m_Origin, m_Size}; }
		float2			GetSize() const { return m_Size; }
		float			GetSize(bool bYAxis) const { return bYAxis ? m_Size.y : m_Size.x; }

		Point			GetOrigin() const { return m_Origin; }

		// Helper to calculate outer size of a widget
		float2			GetOuterSize() {
			const auto size = GetSize();
			const auto margins = GetMargins();
			return {size.x + margins.Left + margins.Right, size.y + margins.Top + margins.Bottom};
		}

	public:

		// Helper
		void			NotifyParentOnSizeChanged(int inAxis = 2) {
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

		void			NotifyParentOnVisibilityChanged() {
			auto parent = GetNearestLayoutParent();
			if(!parent) return;

			ChildLayoutEvent onChild;
			onChild.Child = this;
			onChild.Size = m_Size;
			onChild.Subtype = ChildLayoutEvent::OnVisibility;
			parent->OnEvent(&onChild);
		}

		// Helper to update our size to parent when ParentLayout event comes
		// May be redirected from subclasses
		// @return true if our size has actually changed
		bool			ExpandToParent(const ParentLayoutEvent* inEvent) {
			const auto prevSize = GetSize();
			const auto margins = GetMargins();
			SetOrigin(inEvent->Constraints.TL() + margins.TL());

			for(auto axis = 0; axis != 2; ++axis) {

				if(GetAxisMode()[axis] == AxisMode::Expand) {
					SetSize(axis, inEvent->Constraints.Size()[axis] - margins.Size()[axis]);
				}
			}
			return GetSize() != prevSize;
		}

	public:
		// Margins of a widget which have StyleClass
		virtual Margin  GetMargins() const { return {}; }

		// Margins of a widget which have StyleClass
		virtual Padding GetPaddings() const { return {}; }

	private:
		// Position in pixels relative to parent origin
		Point			m_Origin;
		float2			m_Size;
	};





	/*
	* LayoutWidgett which contains children
	*/
	class Container: public LayoutWidget {
		DEFINE_CLASS_META(Container, LayoutWidget)
	public:

		Container(const std::string& inID = {}): LayoutWidget(inID) {}

		virtual bool DispatchToChildren(IEvent* inEvent) = 0;

		bool OnEvent(IEvent* inEvent) override {
			
			if(inEvent->GetCategory() == EventCategory::Intent) {
				return DispatchToParent(inEvent);

			} else if(inEvent->IsA<HitTestEvent>()) {
				// Dispatch HitTest only if we was hit
				auto bHandled = Super::OnEvent(inEvent);
				if(bHandled) DispatchToChildren(inEvent);				
				return bHandled;

			} else if(inEvent->IsA<ParentLayoutEvent>()) {
				Super::OnEvent(inEvent);
				return true;

			} else if(inEvent->GetCategory() == EventCategory::Debug) {
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

		SingleChildContainer(const std::string& inID = {})
			: Container(inID)
			, ChildSlot(this)
		{}

		// Called by a subclass or a widget which wants to be a child
		void ParentChild(Widget* inChild, WidgetAttachSlot*) override {
			ChildSlot.Parent(inChild);
		}

		bool DispatchToChildren(IEvent* inEvent) override {
			if(ChildSlot) {
				return ChildSlot->OnEvent(inEvent);
			}
			return false;
		}

		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive = true) final {
			if(!ChildSlot) return VisitResultContinue;

			const auto result = inVisitor(ChildSlot.Get());
			if(!result.bContinue) return VisitResultExit;

			if(bRecursive && !result.bSkipChildren) {
				const auto result = ChildSlot->VisitChildren(inVisitor, bRecursive);
				if(!result.bContinue) return VisitResultExit;
			}
			return VisitResultContinue;
		}

		bool OnEvent(IEvent* inEvent) override {

			if(inEvent->GetCategory() == EventCategory::Intent) {
				return DispatchToParent(inEvent);

			} else if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
				const auto bSizeChanged = HandleChildEvent(event);

				if(bSizeChanged) {
					NotifyParentOnSizeChanged();
				}
				return true;
			}
			return Super::OnEvent(inEvent);
		}

	protected:

		// Update child's size if its expanded
		void NotifyChildOnSizeChanged() {
			if(!ChildSlot) return;
			const auto paddings = GetPaddings();

			ParentLayoutEvent layoutEvent;
			layoutEvent.Parent = this;
			layoutEvent.Constraints = Rect(paddings.TL(), GetSize() - paddings.Size());
			ChildSlot->OnEvent(&layoutEvent);
		}

		// Centers our child when child size changes
		void CenterChild(ChildLayoutEvent* inEvent) {
			const auto childSize = inEvent->Child->GetSize();
			const auto position = (Super::GetSize() - childSize) * 0.5f;
			inEvent->Child->SetOrigin(position);
		}

		// Centers our child when parent layout changes
		void CenterChild(ParentLayoutEvent* inEvent) {

			if(auto* child = Super::GetChild<LayoutWidget>()) {
				const auto childOuterSize = child->GetSize() + child->GetMargins().Size();
				const auto offset = (inEvent->Constraints.Size() - childOuterSize) * 0.5f;
				// Pass tight constraints to force child position
				ParentLayoutEvent onParent(this, Rect(offset, childOuterSize));
				child->OnEvent(&onParent);
			}
		}

		// Dispatches draw event to children adding global offset
		void DispatchDrawToChildren(DrawEvent* inEvent) {
			auto eventCopy = *inEvent;
			eventCopy.ParentOriginGlobal += Super::GetOrigin();
			DispatchToChildren(&eventCopy);
		}

		// Helper to update our size based on AxisMode
		// @param bUpdateChildOnAdded if true, updates a newly added child
		// @return true if our size was changed by the child
		bool HandleChildEvent(const ChildLayoutEvent* inEvent, bool bUpdateChildOnAdded = true) {
			const auto axisMode = GetAxisMode();
			const auto childAxisMode = inEvent->Child->GetAxisMode();
			const auto paddings = GetPaddings();
			const auto paddingsSize = paddings.Size();
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

						SetSize(axis, inEvent->Size[axis] + paddingsSize[axis]);
						bSizeChanged = true;
					}
				}

			} else if(inEvent->Subtype == ChildLayoutEvent::OnAdded) {
				bool bUpdateChild = false;

				for(auto axis = 0; axis != 2; ++axis) {

					if(axisMode[axis] == AxisMode::Shrink) {
						Assertf(childAxisMode[axis] == AxisMode::Shrink,
								"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked. "
								"Child: {}, Parent: {}", inEvent->Child->GetDebugIDString(), GetDebugIDString());

						SetSize(axis, inEvent->Size[axis] + paddingsSize[axis]);
						bSizeChanged = true;
					}
				}

				// Update child's position based on padding
				if(bUpdateChildOnAdded) {
					ParentLayoutEvent onParent;
					onParent.Constraints = Rect(paddings.TL(), GetSize() - paddingsSize);
					inEvent->Child->OnEvent(&onParent);
				}
			}
			return bSizeChanged;
		}
	
	public:
		SingleWidgetSlot<> ChildSlot;
	};



	/*
	* LayoutWidget container that has multiple children
	*/
	class MultiChildContainer: public Container {
		DEFINE_CLASS_META(MultiChildContainer, Container)
	public:

		MultiChildContainer(const std::string& inID = {})
			: Container(inID) 
			, ChildrenSlot(this)
		{}

		bool DispatchToChildren(IEvent* inEvent) override {
			for(auto& child : ChildrenSlot.Get()) {
				auto result = child->OnEvent(inEvent);

				if(result && !inEvent->IsBroadcast()) {
					return result;
				}
			}
			return false;
		}

		void GetVisibleChildren(std::vector<LayoutWidget*>* outVisibleChildren) {
			outVisibleChildren->reserve(ChildrenSlot.Size());

			for(auto& child : ChildrenSlot.Get()) {
				auto layoutChild = child->Cast<LayoutWidget>();

				if(!layoutChild) {
					layoutChild = child->GetChild<LayoutWidget>();
				}

				if(layoutChild && layoutChild->IsVisible()) {
					outVisibleChildren->push_back(layoutChild);
				}
			}
		}

		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive = true) final {

			for(auto& child : ChildrenSlot.Get()) {
				const auto result = inVisitor(child.get());
				if(!result.bContinue) return VisitResultExit;

				if(bRecursive && !result.bSkipChildren) {
					const auto result = child->VisitChildren(inVisitor, bRecursive);
					if(!result.bContinue) return VisitResultExit;
				}
			}
			return VisitResultContinue;
		}

		bool OnEvent(IEvent* inEvent) override {

			if(inEvent->GetCategory() == EventCategory::Intent) {
				return DispatchToParent(inEvent);
			}
			Assertf(!inEvent->IsA<ChildLayoutEvent>(), "ChildLayoutEvent is not handled by the MultiChildContainer subclass. Subclass is {}", GetClassName());
			return Super::OnEvent(inEvent);
		}

	public:
		MultiChildSlot ChildrenSlot;
	};

}