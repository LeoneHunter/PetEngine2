#pragma once
#include <functional>

#include "runtime/core/core.h"
#include "runtime/core/util.h"
#include "style.h"

using Point = Vec2<float>;

namespace UI { class Widget; }

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

		Object(const std::string& inClassName, ObjectID inObjectID, Object* inParent)
			: m_DebugName(inClassName)
			, m_ObjectID(inObjectID) 
			, m_Parent(inParent)
		{}

		void PushProperty(std::string_view inName, const std::string& inValue) {
			m_Properties.emplace_back(inName, inValue);
		}

		Object* EmplaceChild(const std::string& inClassName, ObjectID inObjectID) {
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
		std::string			m_DebugName;
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

	void PushObject(const std::string& inDebugName, UI::Widget* inWidget, UI::Widget* inParent) {

		if(!inParent || !m_CursorObject) {
			m_CursorObject = &*m_RootObjects.emplace_back(new Object(inDebugName, (ObjectID)inWidget, nullptr));
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
		m_CursorObject = m_CursorObject->EmplaceChild(inDebugName, (ObjectID)inWidget);
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


// Custom dynamic cast and weak pointer functions
#define WIDGET_CLASS(className, superClassName)\
	DEFINE_CLASS_META(className, superClassName)\
	WeakPtr<className>			GetWeak() {\
		return superClassName::GetWeakAs<className>();\
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

	template<typename T>
	using WeakPtr = util::WeakPtr<T>;
	using WidgetWeakPtr = util::WeakPtr<Widget>;

	// User provided function used to spawn other widgets
	// Used in a tooltip and dragdrop 
	using SpawnerFunction = std::function<LayoutWidget*()>;

	using Margins = RectSides<u32>;
	using Paddings = RectSides<u32>;
	using WidgetSlot = size_t;

	constexpr auto defaultWidgetSlot = WidgetSlot();

	//enum class Axis { X, Y, Both };
	//constexpr AxisIndex ToAxisIndex(Axis inAxis) { assert(inAxis != Axis::Both); return inAxis == Axis::X ? AxisX : AxisY; }
	//constexpr Axis ToAxis(int inAxisIndex) { assert(inAxisIndex == 0 || inAxisIndex == 1); return inAxisIndex ? Axis::Y : Axis::X; }

	// Represent invalid point. I.e. null object for poins
	constexpr auto NOPOINT = Point(std::numeric_limits<float>::max());

	enum class EventCategory {
		Debug, 		// DebugLog
		System, 	// Draw, MouseMove, MouseButton, KeyboardKey
		Layout, 	// HitTest, ParentLayout, ChildLayout, Hover
		Callback,
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

	enum class MouseState {
		Default,	// When hovering an empty space or widget without a tooltip
		Tooltip,	// After some time hovering
		Held,		// When pressed and holding
		Dragged,	// When dragged threshold crossed
		DragDrop,	// When DragDrop active
	};

	enum KeyModifiers {
		LeftShift,
		RightShift,
		LeftControl,
		RightControl,
		LeftAlt,
		RightAlt,
		CapsLock,
		Count,
	};

	using KeyModifiersArray = std::array<bool, (int)KeyModifiers::Count>;

	using TimerCallback = std::function<bool()>;
	using TimerHandle = void*;



	/*
	* Some global state information of the application
	* Can be accesed at any time in read only mode
	* Users should not cache this because its valid only for one frame
	*/
	struct FrameState {
		Point					mousePosGlobal;
		// Number of buttons currently held
		u8						mouseButtonHeldNum = 0;
		MouseButtonMask			mouseButtonsPressedBitField = MouseButtonMask::None;
		// Position of the mouse cursor when the first mouse button has been pressed
		Point					mousePosOnCaptureGlobal;
		KeyModifiersArray		keyModifiersState{false};
		float2					windowSize;
		Theme* 					theme = nullptr;
	};

	/*
	* Controls the order of the root parented widgets processing and drawing
	*/
	enum class Layer {
		Background,
		Float,
		Popup,
		Overlay,
	};

	/*
	* Creates new OS window and a custor renderer for widgets
	* Handles OS input events and dispatches them to widgets
	* Then draw widgets via custom renderer
	* Uses ImGui drawlist and renderer as a backend
	*/
	class Application {
	public:

		virtual ~Application() = default;

		static Application* 	Create(std::string_view inWindowTitle, u32 inWidth, u32 inHeight);
		static Application* 	Get();
		static FrameState		GetFrameState();

		virtual void			Shutdown() = 0;
		// For now we do all ui here
		virtual bool			Tick() = 0;
		// Parents a widget to the root of the widget tree
		// Widgets parented to the root behave like windows and can be reordered
		virtual void			Parent(Widget* inWidget, Layer inLayer) = 0;
		virtual void 			Orphan(Widget* inWidget) = 0;
		virtual void			BringToFront(Widget* inWidget) = 0;

		virtual Theme* 			GetTheme() = 0;
		// Merges inTheme styles with the prevous theme overriding existent styles 
		virtual void			SetTheme(Theme* inTheme) = 0;

		virtual TimerHandle		AddTimer(Widget* inWidget, const TimerCallback& inCallback, u64 inPeriodMs) = 0;
		virtual void			RemoveTimer(TimerHandle inHandle) = 0;
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
		
		// Returns a debug identifier of this object
		// [ClassName: <4 bytes of adress>]
		std::string GetDebugID() { return std::format("[{}: {:x}]", GetClassName(), (uintptr_t)this & 0xffff); }
	};

	// Custom dynamic cast
	#define EVENT_CLASS(className, superClassName) DEFINE_CLASS_META(className, superClassName)

	/*
	* Request to draw a widget debug data
	*/
	class DebugLogEvent final: public IEvent {
		EVENT_CLASS(DebugLogEvent, IEvent)
	public:
		bool IsBroadcast() const override { return true; };
		EventCategory GetCategory() const override { return EventCategory::Debug; }
		PropertyArchive* archive = nullptr;
	};

	/*
	* Only a top Window widget receives this event
	* Children widget receive HoverEvent and MouseDragEvent
	*/
	class MouseDragEvent final: public IEvent {
		EVENT_CLASS(MouseDragEvent, IEvent)
	public:
		EventCategory GetCategory() const override { return EventCategory::System; }

		// If during this event mouse button is being held
		// This is the point of initial mouse press
		Point			mousePosOnCaptureLocal;
		// Position of the mouse inside the hovered widget
		// Relative to the widget size
		Point			mousePosOnCaptureInternal;
		Point			mousePosLocal;		
		float2			mouseDelta;
		MouseButtonMask	mouseButtonsPressedBitField = MouseButtonMask::None;
	};

	/*
	* Passed to children of a widget when it's layout needs update
	* Widgets that are not subclasses of LayoutWidget do not care about these events
	* Constraints define the area in which a child can position itself and change size
	*/
	class ParentLayoutEvent final: public IEvent {
		EVENT_CLASS(ParentLayoutEvent, IEvent)
	public:

		ParentLayoutEvent()
			: parent(nullptr)
		{}

		ParentLayoutEvent(LayoutWidget*	inParent, Rect inConstraints)
			: parent(inParent)
			, constraints(inConstraints)
		{}

		EventCategory	GetCategory() const override { return EventCategory::Layout; }

		LayoutWidget*	parent;
		Rect			constraints;
	};

	/*
	* Passed to the parent by a LayoutWidget widget
	* when its added, modified or deleted to update parent's and itself's layout
	* Widgets that are not subclasses of LayoutWidget do not care about these events
	*/
	class ChildLayoutEvent final: public IEvent {
		EVENT_CLASS(ChildLayoutEvent, IEvent)
	public:

		// What child change spawned the event
		enum Subtype {
			OnAdded,
			OnRemoved,
			OnChanged,
			OnVisibility
		};

		ChildLayoutEvent()
			: subtype(OnChanged)
			, bAxisChanged(true, true)
			, child(nullptr)
		{}

		ChildLayoutEvent(LayoutWidget* inChild,
				   float2 inSize, 
				   Vec2<bool> inAxisChanged = {true, true}, 
				   Subtype inType = OnChanged)
			: subtype(inType)
			, bAxisChanged(inAxisChanged)
			, size(inSize)
			, child(inChild)
		{}

		EventCategory GetCategory() const override { return EventCategory::Layout; }

		Subtype			subtype;
		Vec2<bool>		bAxisChanged;
		float2			size;
		LayoutWidget*	child;
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
			WeakPtr<LayoutWidget>	widget;
			Point					hitPosLocal;
		};

	public:
		constexpr void Push(LayoutWidget* inWidget, Point inPosLocal);

		constexpr bool Empty() const { return stack.empty(); }

		constexpr HitData& Top() { return stack.back(); }
		constexpr LayoutWidget*  TopWidget() { return stack.empty() ? nullptr : stack.back().widget.GetChecked(); }
		
		// Find hit data for a specified widget
		constexpr const HitData* Find(const LayoutWidget* inWidget) const {
			for(const auto& hitData : stack) {
				if(hitData.widget == inWidget) 
					return &hitData;
			}
			return nullptr;
		}

		constexpr auto begin() { return stack.rbegin(); }
		constexpr auto end() { return stack.rend(); }

	public:
		std::vector<HitData> stack;
	};

	/*
	* Called by the framework when mouse cursor is moving
	*/
	class HitTestEvent final: public IEvent {
		EVENT_CLASS(HitTestEvent, IEvent)
	public:

		// inInternalPos is a position inside the widget rect relative to origin
		constexpr void PushItem(LayoutWidget* inWidget, Point inInternalPos) {
			hitStack.Push(inWidget, inInternalPos);
		}

		constexpr Point GetLastHitPos() {
			return hitStack.Empty() ? hitPosGlobal : hitStack.Top().hitPosLocal;
		}

		EventCategory GetCategory() const override { return EventCategory::Layout; }

	public:
		Point		hitPosGlobal;
		HitStack	hitStack;
	};



	/*
	* Dispatched to the last widget in the hittest stack
	* If the widget doen't want to handle the event it passes it to the parent
	* The dispatched also responsible for handling hover out events
	*/
	class HoverEvent final: public IEvent {
		EVENT_CLASS(HoverEvent, IEvent)
	public:

		static HoverEvent EnterEvent() { 
			HoverEvent e; 
			e.bHoverEnter = true;
			e.bHoverLeave = false;
			return e; 
		}

		static HoverEvent LeaveEvent() { 
			HoverEvent e; 
			e.bHoverEnter = false;
			e.bHoverLeave = true;
			return e; 
		}

		static HoverEvent Normal() { 
			HoverEvent e; 
			e.bHoverEnter = false;
			e.bHoverLeave = false;
			return e; 
		}

		EventCategory GetCategory() const override { return EventCategory::System; }

		u8 bHoverEnter: 1;
		u8 bHoverLeave: 1;
	};

	

	/*
	* System event dispatched by Application when os window is clicked
	* First its dispatched to the top widget of the hitstack until a widget 
	*	that returns true is found. Then [handled] is set to true and event
	*	continues to propagate up the tree so that other widget and wrappers can 
	*	handle it differently.
	*/
	class MouseButtonEvent final: public IEvent {
		EVENT_CLASS(MouseButtonEvent, IEvent)
	public:

		EventCategory GetCategory() const override { return EventCategory::System; }

		// Set to true when OnEvent() returns true
		bool 		bHandled = false;
		MouseButton	button = MouseButton::None;
		bool		bButtonPressed = false;
		Point		mousePosGlobal;
		Point		mousePosLocal;
	};




	class Drawlist {
	public:

		virtual ~Drawlist() = default;
		virtual void PushBox(Rect inRect, const BoxStyle* inStyle) = 0;
		virtual void PushBox(Rect inRect, Color inColor, bool bFilled = true) = 0;

		virtual void PushText(Point inOrigin, const TextStyle* inStyle, std::string_view inTextView) = 0;
		virtual void PushClipRect(Rect inClipRect) = 0;
		virtual void PopClipRect() = 0;

		virtual void PushTransform(float2 inTransform) = 0;
		virtual void PopTransform() = 0;
	};

	class DrawEvent final: public IEvent {
		EVENT_CLASS(DrawEvent, IEvent)
	public:

		bool IsBroadcast() const override { return true; };
		EventCategory GetCategory() const override { return EventCategory::System; }

		Drawlist*	drawList = nullptr;
		Theme*		theme = nullptr;
	};






	struct AxisMode {

		enum Mode {
			Expand, 
			Shrink
		};

		constexpr Mode operator[] (Axis inAxis) const {
			return inAxis == Axis::X ? x : y;
		}

		Mode x;
		Mode y;
	};

	constexpr auto axisModeExpand = AxisMode(AxisMode::Expand, AxisMode::Expand);
	constexpr auto axisModeShrink = AxisMode(AxisMode::Shrink, AxisMode::Shrink);

	enum class WidgetFlags {
		None		= 0x0,
		Hidden		= 0x01,
		AxisXExpand	= 0x02,
		AxisYExpand	= 0x04,
		// Position of the widget doesn't depend on parent constraints
		FloatLayout		= 0x08,
	};
	DEFINE_ENUM_FLAGS_OPERATORS(WidgetFlags)	
	DEFINE_ENUMFLAGS_TOSTRING_5(WidgetFlags, None, Hidden, AxisXExpand, AxisYExpand, FloatLayout)
	

	// Returned by the callback to instruct iteration
	struct VisitResult {
		// If false stops iteration and exit from all visit calls
		bool bContinue = true;
		// Skip iterating children of a current widget
		bool bSkipChildren = false;		
	};
	constexpr auto visitResultContinue = VisitResult(true, false);
	constexpr auto visitResultExit = VisitResult(false, false);
	constexpr auto visitResultSkipChildren = VisitResult(true, true);
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

		template<class T = Widget>
		WeakPtr<T>			GetWeakAs() {
			if(this == nullptr) return {};
			Assertf(this->Cast<T>(), "Cannot cast {} to the class {}", GetDebugID(), T::GetStaticClassName());

			if(!m_RefCounter) {
				m_RefCounter = new util::RefCounter();
			}
			return WeakPtr<T>{this->Cast<T>(), m_RefCounter};
		}

		WeakPtr<Widget>		GetWeak() { return GetWeakAs<Widget>(); }

		virtual				~Widget() {
			if(m_RefCounter) {
				m_RefCounter->OnDestructed();
			}
		}

		// Notify parents that we are about to be destructed
		virtual void		Destroy() {
			GetParent()->Orphan(this);
			delete this;
		}
		
		// Calls a visitor callback on this object and children if bRecursive == true
		virtual VisitResult	VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive = true) { return visitResultContinue; }

		// Visits parents in the tree recursively
		void				VisitParent(const WidgetVisitor& inVisitor, bool bRecursive = true) {
			for(auto parent = m_Parent; parent; parent = parent->GetParent()) {
				const auto result = inVisitor(parent);
				if(!result.bContinue || !bRecursive) return;
			}
		}

		// Attaches this widget to a parent slot
		virtual void		Parent(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) { Assertf(false, "Class {} cannot have children.", GetClassName()); }
		virtual void		Orphan(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) { Assertf(false, "Class {} cannot have children.", GetClassName()); }
		
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
			 
		virtual bool		OnEvent(IEvent* inEvent) {	
			if(auto* event = inEvent->Cast<DebugLogEvent>()) {
				event->archive->PushObject(GetClassName().data(), this, GetParent());
				DebugSerialize(*event->archive);
			}
			return true;
		};

		virtual void		DebugSerialize(PropertyArchive& inArchive) {}

		// Transforms a point in local space into a point in global space
		// recursively iterating parents
		virtual Point		TransformLocalToGlobal(Point inPosition) const { 
			return m_Parent ? m_Parent->TransformLocalToGlobal(inPosition) : inPosition; 
		}

		bool				DispatchToParent(IEvent* inEvent) { if(m_Parent) { return m_Parent->OnEvent(inEvent); } return false; }		

	public:

		StringID			GetID() const { return m_ID; }

		Widget*				GetParent() { return m_Parent; }
		const Widget*		GetParent() const { return m_Parent; }

		// Gets nearest parent in the tree of the class T
		template<WidgetSubclass T>
		const T*			FindParentOfClass() const {
			for(auto* parent = GetParent(); parent; parent = parent->GetParent()) {
				if(parent->IsA<T>()) 
					return static_cast<const T*>(parent);
			}
			return nullptr;
		}

		template<WidgetSubclass T>
		T*					FindParentOfClass() { return const_cast<T*>(std::as_const(*this).FindParentOfClass<T>()); }

		// Looks for a child with specified class
		// @param bRecursive - Looks among children of children
		template<WidgetSubclass T>
		T*					FindChildOfClass(bool bRecursive = true) {
			T* child = nullptr;
			VisitChildren([&](Widget* inChild) {
				if(inChild->IsA<T>()) {
					child = static_cast<T*>(inChild);
					return visitResultExit; 
				}
				return visitResultContinue;
			},
			bRecursive);
			return child;
		}

		// Returns a debug identifier of this object
		// [MyWidgetClassName: <4 bytes of adress> "MyObjectID"]
		std::string			GetDebugID() {
			if(GetID()) {
				return std::format("[{}: {:x} \"{}\"]", GetClassName(), (uintptr_t)this & 0xffff, GetID());
			} else {
				return std::format("[{}: {:x}]", GetClassName(), (uintptr_t)this & 0xffff);
			}
		}

	protected:
		Widget(const std::string& inID = {})
			: m_ID(inID)
			, m_Parent(nullptr)
			, m_RefCounter(nullptr)			
		{}

		Widget(const Widget&) = delete;
		Widget& operator=(const Widget&) = delete;

	private:
		// Optional user defined ID
		StringID			m_ID;
		// Our parent widget who manages our lifetime and passes events
		Widget*				m_Parent;
		// Used for custom WeakPtr to this
		util::RefCounter*	m_RefCounter;
	};
	

	/*
	* Abstract widget container that has one child
	*/
	class SingleChildWidget: public Widget {
		WIDGET_CLASS(SingleChildWidget, Widget)
	public:

		void Parent(Widget* inChild, WidgetSlot inSlot = defaultWidgetSlot) override {
			Assert(inChild && !inChild->GetParent() && !m_Child);
			m_Child.reset(inChild);
			m_Child->OnParented(this);
		}
		void SetChild(Widget* inWidget) { Parent(inWidget); }

		void Orphan(Widget* inChild, WidgetSlot inSlot = defaultWidgetSlot) override {
			Assert(inChild);
			if(m_Child.get() == inChild) {
				m_Child.release();
				m_Child->OnOrphaned();
			}
		}

		void OnParented(Widget* inParent) override;
		void OnOrphaned() override;

		bool DispatchToChildren(IEvent* inEvent) {
			if(m_Child) {
				return m_Child->OnEvent(inEvent);
			}
			return false;
		}

		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive = true) final {
			if(!m_Child) return visitResultContinue;

			const auto result = inVisitor(m_Child.get());
			if(!result.bContinue) return visitResultExit;

			if(bRecursive && !result.bSkipChildren) {
				const auto result = m_Child->VisitChildren(inVisitor, bRecursive);
				if(!result.bContinue) return visitResultExit;
			}
			return visitResultContinue;
		}
		
		bool OnEvent(IEvent* inEvent) override {
			
			if(auto* event = inEvent->Cast<DebugLogEvent>()) {
				event->archive->PushObject(GetDebugID(), this, GetParent());
				DebugSerialize(*event->archive);
				DispatchToChildren(inEvent);
				return true;
			} 
			
			if(inEvent->IsA<DrawEvent>()) {
				DispatchToChildren(inEvent);
				return true;
			}
			return Super::OnEvent(inEvent);
		};

	protected:
		SingleChildWidget(const std::string& inID = {})
			: Widget(inID)
			, m_Child()
		{}

	private:
		std::unique_ptr<Widget> m_Child;
	};



	/*
	* Subclasses of this widget hold the state of a subtree
	* Usually the topmost widget in the group of widget that control its descendants
	* Custom user widget should be inherited from this
	*/
	class StatefulWidget: public SingleChildWidget {
		WIDGET_CLASS(StatefulWidget, SingleChildWidget)
	public:

		bool OnEvent(IEvent* inEvent) override {
						
			if(inEvent->IsA<ChildLayoutEvent>()) {
				return DispatchToParent(inEvent);
			} 
			
			if(inEvent->GetCategory() == EventCategory::Debug) {
				return Super::OnEvent(inEvent);
			}
			return DispatchToChildren(inEvent);
		};

	protected:
		StatefulWidget(const std::string& inID = {})
			: SingleChildWidget(inID)
		{}		
	};



	/*
	* A widget which has size and position
	* And possibly can be drawn
	*/
	class LayoutWidget: public Widget {
		WIDGET_CLASS(LayoutWidget, Widget)
	public:

		void OnParented(Widget* inParent) override {
			Super::OnParented(inParent);
			auto* parent = FindParentOfClass<LayoutWidget>();

			if(parent) {
				ChildLayoutEvent onChild;
				onChild.child = this;
				onChild.size = m_Size;
				onChild.subtype = ChildLayoutEvent::OnAdded;
				parent->OnEvent(&onChild);
			}
		}

		void OnOrphaned() override {
			auto* parent = FindParentOfClass<LayoutWidget>();

			if(parent) {
				ChildLayoutEvent onChild;
				onChild.child = this;
				onChild.size = m_Size;
				onChild.subtype = ChildLayoutEvent::OnRemoved;
				parent->OnEvent(&onChild);
			}
			Super::OnOrphaned();
		}

		bool OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->Cast<HitTestEvent>()) {
				if(!IsVisible()) return false;
				auto hitPosLocalSpace = event->GetLastHitPos();

				if(GetRect().Contains(hitPosLocalSpace)) {
					event->PushItem(this, hitPosLocalSpace - GetOrigin());
					return true;
				}
				return false;
			} 
			
			if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
				HandleParentLayoutEvent(event);
				return true;
			} 
			
			if(auto* event = inEvent->Cast<DebugLogEvent>()) {
				event->archive->PushObject(GetDebugID(), this, GetParent());
				DebugSerialize(*event->archive);
				return true;
			} 

			if(inEvent->IsA<DrawEvent>()) {
				Assertm(false, "Draw event not handled nor dispatched to children!");
				return true;
			}
			return false;
		};

		void DebugSerialize(PropertyArchive& inArchive) override {
			Super::DebugSerialize(inArchive);
			inArchive.PushProperty("Flags", m_Flags);
			inArchive.PushProperty("Origin", m_Origin);
			inArchive.PushProperty("Size", m_Size);
		}

		Point TransformLocalToGlobal(Point inLocalPos) const override {
			return GetParent() ? GetParent()->TransformLocalToGlobal(inLocalPos + m_Origin) : inLocalPos + m_Origin;
		}

	public:

		// Looks for a nearest LayoutWidget down the tree starting from inWidget
		static LayoutWidget* FindNearest(Widget* inWidget) {
			if(inWidget->IsA<LayoutWidget>())
				return inWidget->Cast<LayoutWidget>();
			return inWidget->FindChildOfClass<LayoutWidget>();
		}

		// Finds the topmost widget up the tree that is not another layout widget
		// Because LayoutWidget serves as a base for widgets composition
		// Other widgets that wrap this widget will use it for hovering, clicking, moving behaviour
		Widget*			FindTopmost() {
			Widget* topmost = this;

			for(auto* parent = GetParent();
				parent && !parent->IsA<LayoutWidget>();
				parent = parent->GetParent()) {
				topmost = parent;
			}
			return topmost;
		}

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

		void			SetAxisMode(Axis inAxis, AxisMode::Mode inMode) {
			if(inAxis == Axis::X) {
				inMode == AxisMode::Mode::Expand
					? SetFlags(WidgetFlags::AxisXExpand)
					: ClearFlags(WidgetFlags::AxisXExpand);
			} else if(inAxis == Axis::Y) {
				inMode == AxisMode::Mode::Expand
					? SetFlags(WidgetFlags::AxisYExpand)
					: ClearFlags(WidgetFlags::AxisYExpand);
			}
		}

		// Hiddent objects won't draw themselves and won't handle hovering 
		// but layout update should be managed by the parent
		void 			SetVisibility(bool bVisible) { bVisible ? ClearFlags(WidgetFlags::Hidden) : SetFlags(WidgetFlags::Hidden); }
		bool			IsVisible() const { return !HasAnyFlags(WidgetFlags::Hidden); }

		// Should be called by subclasses
		void			SetOrigin(Point inPos) { m_Origin = inPos; }
		void			SetOrigin(float inX, float inY) { m_Origin = {inX, inY}; }
		void			SetOrigin(Axis inAxis, float inPos) { m_Origin[inAxis] = inPos; }

		void			SetSize(float2 inSize) { m_Size = inSize; }
		void			SetSize(Axis inAxis, float inSize) { m_Size[(int)inAxis] = inSize; }
		void			SetSize(float inX, float inY) { m_Size = {inX, inY}; }

		Rect			GetRect() const { return {m_Origin, m_Size}; }
		// Returns Rect of this widget in global coordinates
		Rect			GetRectGlobal() const { return Rect(GetParent() ? GetParent()->TransformLocalToGlobal(m_Origin) : m_Origin, m_Size); }
		float2			GetSize() const { return m_Size; }
		float			GetSize(Axis inAxis) const { return m_Size[inAxis]; }

		Point			GetOrigin() const { return m_Origin; }

		// Helper to calculate outer size of a widget
		float2			GetOuterSize() {
			const auto size = GetSize();
			const auto margins = GetLayoutStyle() ? GetLayoutStyle()->margins : Margins{};
			return {size.x + margins.Left + margins.Right, size.y + margins.Top + margins.Bottom};
		}

		bool			HasAnyFlags(WidgetFlags inFlags) const { return m_Flags & inFlags; }

		void				SetLayoutStyle(const LayoutStyle* inStyle) { m_LayoutStyle = inStyle; }
		const LayoutStyle*	GetLayoutStyle() const { return m_LayoutStyle; }

		// Widget's position won't be affected by parent's layout events
		void			SetFloatLayout(bool bEnable) {  bEnable ? SetFlags(WidgetFlags::FloatLayout) : ClearFlags(WidgetFlags::FloatLayout); }

	private:
		void			SetFlags(WidgetFlags inFlags) { m_Flags |= inFlags; }
		void			ClearFlags(WidgetFlags inFlags) { m_Flags &= ~inFlags; }

	public:
		// Helper
		void			DispatchLayoutToParent(int inAxis = 2) {
			auto parent = FindParentOfClass<LayoutWidget>();
			if(!parent) return;

			ChildLayoutEvent onChild;
			onChild.child = this;
			onChild.size = m_Size;
			onChild.subtype = ChildLayoutEvent::OnChanged;

			if(inAxis == 2) {
				onChild.bAxisChanged = {true, true};
			} else {
				onChild.bAxisChanged[inAxis] = true;
			}
			parent->OnEvent(&onChild);
		}

		void			NotifyParentOnVisibilityChanged() {
			auto parent = FindParentOfClass<LayoutWidget>();
			if(!parent) return;

			ChildLayoutEvent onChild;
			onChild.child = this;
			onChild.size = m_Size;
			onChild.subtype = ChildLayoutEvent::OnVisibility;
			parent->OnEvent(&onChild);
		}

		// Helper to update our size to parent when ParentLayout event comes
		// May be redirected from subclasses
		// @return true if our size has actually changed
		bool			HandleParentLayoutEvent(const ParentLayoutEvent* inEvent) {
			if(HasAnyFlags(WidgetFlags::FloatLayout)) return false;

			const auto prevSize = GetSize();
			const auto margins = GetLayoutStyle() ? GetLayoutStyle()->margins : Margins{};
			SetOrigin(inEvent->constraints.TL() + margins.TL());

			for(auto axis: Axes2D) {
				if(GetAxisMode()[axis] == AxisMode::Expand) {
					SetSize(axis, inEvent->constraints.Size()[axis] - margins.Size()[axis]);
				}
			}
			return GetSize() != prevSize;
		}

	protected:
		LayoutWidget(const LayoutStyle* inStyle = nullptr,
					 AxisMode			inAxisMode = axisModeShrink,
					 const std::string& inID = {})
			: Widget(inID) 
			, m_Flags(WidgetFlags::None)
			, m_LayoutStyle(inStyle) {
			SetAxisMode(inAxisMode);
		}

	private:
		// Position in pixels relative to parent origin
		Point				m_Origin;
		float2				m_Size;
		WidgetFlags			m_Flags;
		// Style object in the Theme
		const LayoutStyle*	m_LayoutStyle;
	};


	/*
	* LayoutWidget container that has one child
	*/
	class SingleChildLayoutWidget: public LayoutWidget {
		WIDGET_CLASS(SingleChildLayoutWidget, LayoutWidget)
	public:		

		void Parent(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) override {
			Assert(inWidget && !inWidget->GetParent() && !m_Child);
			m_Child.reset(inWidget);
			m_Child->OnParented(this);
			DispatchLayoutToChildren();
		}
		void SetChild(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) { Parent(inWidget, inSlot); }

		void Orphan(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) override {
			Assert(inWidget);
			if(m_Child.get() == inWidget) {
				m_Child.release();
				m_Child->OnOrphaned();
			}
		}

		bool DispatchToChildren(IEvent* inEvent) {
			if(m_Child) {
				return m_Child->OnEvent(inEvent);
			}
			return false;
		}

		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive = true) final {
			if(!m_Child) return visitResultContinue;

			const auto result = inVisitor(m_Child.get());
			if(!result.bContinue) return visitResultExit;

			if(bRecursive && !result.bSkipChildren) {
				const auto result = m_Child->VisitChildren(inVisitor, bRecursive);
				if(!result.bContinue) return visitResultExit;
			}
			return visitResultContinue;
		}

		bool OnEvent(IEvent* inEvent) override {

			if(auto* layoutEvent = inEvent->Cast<ParentLayoutEvent>()) {

				if(HandleParentLayoutEvent(layoutEvent)) {
					DispatchLayoutToChildren();
				}
				return true;
			}
			
			if(auto* childLayoutEvent = inEvent->Cast<ChildLayoutEvent>()) {

				if(HandleChildEvent(childLayoutEvent)) {
					DispatchLayoutToParent();
				}
				return true;
			}

			if(auto* event = inEvent->Cast<DebugLogEvent>()) {
				event->archive->PushObject(GetDebugID(), this, GetParent());
				DebugSerialize(*event->archive);
				DispatchToChildren(inEvent);
				return true;
			} 
			
			if(inEvent->IsA<HitTestEvent>()) {
				
				if(Super::OnEvent(inEvent)) {
					// Dispatch HitTest only if we was hit
					if(auto* layoutChild = FindChildOfClass<LayoutWidget>(this)) {
						layoutChild->OnEvent(inEvent);
					}
					return true;
				} 
				return false;
			} 

			if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
				DispatchDrawToChildren(drawEvent);
				return true;
			}
			return Super::OnEvent(inEvent);
		}

	protected:

		// Centers our child when child size changes
		void CenterChild(ChildLayoutEvent* inEvent) {
			const auto childSize = inEvent->child->GetSize();
			const auto position = (Super::GetSize() - childSize) * 0.5f;
			inEvent->child->SetOrigin(position);
		}

		// Centers our child when parent layout changes
		void CenterChild(ParentLayoutEvent* inEvent) {

			if(auto* child = Super::FindChildOfClass<LayoutWidget>()) {
				const auto margins = GetLayoutStyle() ? GetLayoutStyle()->margins : Margins{};
				const auto childOuterSize = child->GetSize() + margins.Size();
				const auto float2 = (inEvent->constraints.Size() - childOuterSize) * 0.5f;
				// Pass tight constraints to force child position
				ParentLayoutEvent onParent(this, Rect(float2, childOuterSize));
				child->OnEvent(&onParent);
			}
		}

		// Dispatches draw event to children adding global float2
		void DispatchDrawToChildren(DrawEvent* inEvent, float2 inAdditionalOffset = {}) {
			inEvent->drawList->PushTransform(Super::GetOrigin() + inAdditionalOffset);
			DispatchToChildren(inEvent);
			inEvent->drawList->PopTransform();
		}

		// Dispatches event to children adding paddings to constraints
		void DispatchLayoutToChildren() {
			if(!m_Child) return;

			const auto layoutInfo = GetLayoutStyle();
			ParentLayoutEvent layoutEvent;
			layoutEvent.parent = this;
			layoutEvent.constraints = Rect(layoutInfo->paddings.TL(), GetSize() - layoutInfo->paddings.Size());

			if(auto* layoutWidget = FindChildOfClass<LayoutWidget>()) {
				LOGF("Widget {} dispatched parent layout event to child {}", GetDebugID(), layoutWidget->GetDebugID());
				layoutWidget->OnEvent(&layoutEvent);
			}
		}

		// Helper to update our size based on AxisMode
		// @param bUpdateChildOnAdded if true, updates a newly added child
		// @return true if our size was changed by the child
		bool HandleChildEvent(const ChildLayoutEvent* inEvent, bool bUpdateChildOnAdded = true) {
			const auto axisMode = GetAxisMode();
			const auto childAxisMode = inEvent->child->GetAxisMode();
			const auto paddings = GetLayoutStyle() ? GetLayoutStyle()->paddings : Paddings{};
			const auto paddingsSize = paddings.Size();
			bool bSizeChanged = false;

			if(inEvent->subtype == ChildLayoutEvent::OnRemoved) {
				// Default behavior
				SetSize({0.f, 0.f});
				bSizeChanged = true;

			} else if(inEvent->subtype == ChildLayoutEvent::OnChanged) {

				for(auto axis: Axes2D) {

					if(inEvent->bAxisChanged[axis] && axisMode[axis] == AxisMode::Shrink) {
						Assertf(childAxisMode[axis] == AxisMode::Shrink,
								"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked."
								"Child class and id: '{}' '{}', Parent class and id: '{}' '{}'", inEvent->child->GetClassName(), inEvent->child->GetID(), GetClassName(), GetID());

						SetSize(axis, inEvent->size[axis] + paddingsSize[axis]);
						bSizeChanged = true;
					}
				}

			} else if(inEvent->subtype == ChildLayoutEvent::OnAdded) {
				bool bUpdateChild = false;

				for(auto axis: Axes2D) {

					if(axisMode[axis] == AxisMode::Shrink) {
						Assertf(childAxisMode[axis] == AxisMode::Shrink,
								"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked. "
								"Child: {}, Parent: {}", inEvent->child->GetDebugID(), GetDebugID());

						SetSize(axis, inEvent->size[axis] + paddingsSize[axis]);
						bSizeChanged = true;
					}
				}

				// Update child's position based on padding
				if(bUpdateChildOnAdded) {
					ParentLayoutEvent onParent;
					onParent.constraints = Rect(paddings.TL(), GetSize() - paddingsSize);
					inEvent->child->OnEvent(&onParent);
				}
			}
			return bSizeChanged;
		}

	protected:	
		SingleChildLayoutWidget(StringID 			inStyleName,
								AxisMode			inAxisMode = axisModeShrink,
								const std::string&	inID = {})
			: LayoutWidget(Application::Get()->GetTheme()->Find(inStyleName)->FindOrDefault<LayoutStyle>(), 
						   inAxisMode, 
						   inID)
			, m_Child()
		{}
	
	public:
		std::unique_ptr<Widget> m_Child;
	};



	/*
	* LayoutWidget container that has multiple children
	*/
	class MultiChildLayoutWidget: public LayoutWidget {
		WIDGET_CLASS(MultiChildLayoutWidget, LayoutWidget)
	public:

		void Parent(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) override {
			Assert(inWidget && !inWidget->GetParent());
			m_Children.emplace_back(inWidget);
			inWidget->OnParented(this);
		}
		void AddChild(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) {
			Parent(inWidget, inSlot);
		}

		void Orphan(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) override {
			Assert(inWidget);

			for(auto it = m_Children.begin(); it != m_Children.end(); ++it) {
				if(it->get() == inWidget) {
					it->release();
					m_Children.erase(it);
					break;
				}
			}			
			inWidget->OnOrphaned();
		}

		bool DispatchToChildren(IEvent* inEvent) {
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
				auto layoutChild = child->Cast<LayoutWidget>();

				if(!layoutChild) {
					layoutChild = child->FindChildOfClass<LayoutWidget>();
				}

				if(layoutChild && layoutChild->IsVisible()) {
					outVisibleChildren->push_back(layoutChild);
				}
			}
		}

		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive = true) final {

			for(auto& child : m_Children) {
				const auto result = inVisitor(child.get());
				if(!result.bContinue) return visitResultExit;

				if(bRecursive && !result.bSkipChildren) {
					const auto result = child->VisitChildren(inVisitor, bRecursive);
					if(!result.bContinue) return visitResultExit;
				}
			}
			return visitResultContinue;
		}

		bool OnEvent(IEvent* inEvent) override {
			Assertf(!inEvent->IsA<ChildLayoutEvent>(), 
					"ChildLayoutEvent is not handled by the MultiChildLayoutWidget subclass. Subclass is {}", GetClassName());

			if(inEvent->IsA<HitTestEvent>()) {
				if(Super::OnEvent(inEvent)) {
					for(auto& child: m_Children) {
						auto* layoutChild = LayoutWidget::FindNearest(child.get());
						if(layoutChild && layoutChild->OnEvent(inEvent)) {
							break;
						}
					}
					return true;
				} 
				return false;
			} 
			if(auto* event = inEvent->Cast<DebugLogEvent>()) {
				event->archive->PushObject(GetDebugID(), this, GetParent());
				DebugSerialize(*event->archive);
				DispatchToChildren(inEvent);
				return true;
			} 
			if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
				drawEvent->drawList->PushTransform(GetOrigin());
				DispatchToChildren(drawEvent);
				drawEvent->drawList->PopTransform();
				return true;
			}
			return Super::OnEvent(inEvent);
		}

	protected:
		MultiChildLayoutWidget(const LayoutStyle* inStyle = nullptr,
							   AxisMode 		  inAxisMode = axisModeShrink,
							   const std::string& inID = {})
			: LayoutWidget(inStyle, inAxisMode, inID)
			, m_Children() 
		{}

	private:
		std::vector<std::unique_ptr<Widget>> m_Children;
	};




	class MouseRegionBuilder;

	using MouseEnterEventCallback = std::function<void()>;
	using MouseLeaveEventCallback = std::function<void()>;
	using MouseHoverEventCallback = std::function<void(const HoverEvent&)>;
	using MouseButtonEventCallback = std::function<void(const MouseButtonEvent&)>;

	struct MouseRegionConfig {
		MouseEnterEventCallback  onMouseEnter;
		MouseLeaveEventCallback  onMouseLeave;
		MouseHoverEventCallback  onMouseHover;
		MouseButtonEventCallback onMouseButton;
		// Whether to handle events even if other MouseRegion widget 
		//     has already handled the event
		bool					 bHandleHoverAlways = false;
		bool					 bHandleButtonAlways = false;
	};

	/*
	* Widget that detects mouse events
	* Allows to receive events even if handled by children
	* Uses underlying LayoutWidget for hit detection
	* Other widget types can still receive these events but only if not captured by children
	*/
	class MouseRegion: public SingleChildWidget {
		WIDGET_CLASS(MouseRegion, SingleChildWidget)
	public:
		static MouseRegionBuilder Build();

		static MouseRegion* New(const MouseRegionConfig& inConfig, Widget* inChild) { 
			Assertf(inConfig.onMouseLeave && (inConfig.onMouseHover || inConfig.onMouseEnter),
					"OnMouseLeave and OnMouseEnter or OnMouseHover listeners should be set.");
			auto* out = new MouseRegion(inConfig); 
			out->SetChild(inChild);
			return out;
		}

		bool OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->Cast<HoverEvent>()) {
				if(event->bHoverEnter) {
					if(m_Config.onMouseEnter) {
						m_Config.onMouseEnter();
					} else {
						m_Config.onMouseHover(*event);
					}
				} else if(event->bHoverLeave) {
					if(m_Config.onMouseLeave) {
						m_Config.onMouseLeave();
					}
				} else if(m_Config.onMouseHover) {
					m_Config.onMouseHover(*event);
				}
				// We're hoverable if either of callbacks is set
				return m_Config.onMouseEnter || m_Config.onMouseHover;
			}

			if(auto* event = inEvent->Cast<MouseButtonEvent>()) {
				if(event->button == MouseButton::ButtonLeft && !event->bHandled) {
					if(event->bButtonPressed) {

					} else {

					}
					return true;
				}
				return false;
			}
			return Super::OnEvent(inEvent);
		}

		bool ShouldAlwaysReceiveHover() const { return m_Config.bHandleHoverAlways; }
		bool ShouldAlwaysReceiveButton() const { return m_Config.bHandleButtonAlways; }

	protected:
		MouseRegion(const MouseRegionConfig& inConfig) 
			: m_Config(inConfig)
		{}
	
	private:	
		MouseRegionConfig m_Config;
	};

	class MouseRegionBuilder {
	public:
		MouseRegionBuilder() {}

		MouseRegionBuilder& OnMouseEnter(const MouseEnterEventCallback& c) { config.onMouseEnter = c; return *this; }
		MouseRegionBuilder& OnMouseLeave(const MouseLeaveEventCallback& c) { config.onMouseLeave = c; return *this; }
		MouseRegionBuilder& OnMouseHover(const MouseHoverEventCallback& c) { config.onMouseHover = c; return *this; }
		MouseRegionBuilder& HandleHoverAlways(bool b = true) { config.bHandleHoverAlways = b; return *this; }
		MouseRegionBuilder& HandleButtonAlways(bool b = true) { config.bHandleButtonAlways = b; return *this; }
		MouseRegionBuilder& Child(Widget* inChild) { child.reset(inChild); return *this; }

		MouseRegion* 		New() { return MouseRegion::New(config, child.release()); }

	private:
		MouseRegionConfig       config;
		std::unique_ptr<Widget> child;
	};

	inline MouseRegionBuilder MouseRegion::Build() { return {}; }

}

inline void UI::SingleChildWidget::OnParented(Widget* inParent) {
	Super::OnParented(inParent);
	LOGF("A widget {} has been parented to {}", GetDebugID(), inParent->GetDebugID());
	if(!inParent->IsA<LayoutWidget>()) return;
	auto* layoutChild = FindChildOfClass<LayoutWidget>();
	// If a layout widget with wrappers is parented
	// we need to notify the parent to update the layout
	if(layoutChild) {
		ChildLayoutEvent e;
		e.child = layoutChild;
		e.subtype = ChildLayoutEvent::OnAdded;
		inParent->OnEvent(&e);
	}
}

inline void UI::SingleChildWidget::OnOrphaned() {
	auto  bParentIsLayout = GetParent()->IsA<LayoutWidget>();
	auto* child = FindChildOfClass<LayoutWidget>();

	if(bParentIsLayout && child) {
		ChildLayoutEvent e;
		e.child = child;
		e.size = child->GetSize();
		e.subtype = ChildLayoutEvent::OnRemoved;
		GetParent()->OnEvent(&e);
	}
	Super::OnOrphaned();
}

constexpr void UI::HitStack::Push(LayoutWidget* inWidget, Point inPosLocal) {
	stack.push_back(HitData{inWidget->GetWeak(), inPosLocal});
}