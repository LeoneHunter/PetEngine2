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

		Property(std::string_view name, const std::string& value)
			: Name(name)
			, Value(value) {}

		std::string_view	Name;
		std::string			Value;
	};

	struct Object {

		Object(const std::string& className, ObjectID objectID, Object* parent)
			: debugName_(className)
			, objectID_(objectID) 
			, parent_(parent)
		{}

		void PushProperty(std::string_view name, const std::string& value) {
			properties_.emplace_back(name, value);
		}

		Object* EmplaceChild(const std::string& className, ObjectID objectID) {
			return  &*children_.emplace_back(new Object(className, objectID, this));
		}

		bool Visit(const Visitor& visitor) {
			const auto bContinue = visitor(*this);
			if(!bContinue) return false;

			for(auto& child : children_) { 
				const auto bContinue = child->Visit(visitor);
				if(!bContinue) return false;
			}
			return true;
		}

		ObjectID			objectID_ = 0;
		std::string			debugName_;
		std::list<Property> properties_;
		Object*				parent_ = nullptr;
		std::list<std::unique_ptr<Object>>	children_;
	};

public:

	// Visit objects recursively in depth first
	// Stops iteration if visitor returns false
	void VisitRecursively(const Visitor& visitor) {
		if(rootObjects_.empty()) return;

		for(auto& child : rootObjects_) { 
			const auto bContinue = child->Visit(visitor);
			if(!bContinue) return;
		}
	}

	void PushObject(const std::string& debugName, UI::Widget* widget, UI::Widget* parent) {
		if(!parent || !cursorObject_) {
			cursorObject_ = &*rootObjects_.emplace_back(new Object(debugName, (ObjectID)widget, nullptr));
			return;
		}
		if(cursorObject_->objectID_ != (ObjectID)parent) {
			for(Object* ancestor = cursorObject_->parent_; ancestor; ancestor = ancestor->parent_) {
				if(ancestor->objectID_ == (ObjectID)parent) {
					cursorObject_ = ancestor;
				}
			}
		}
		assert(cursorObject_);
		cursorObject_ = cursorObject_->EmplaceChild(debugName, (ObjectID)widget);
	}

	// Push formatter property string
	void PushProperty(std::string_view name, Point property) {
		Assertm(cursorObject_, "PushObject() should be called first");
		Assert(!name.empty());
		cursorObject_->PushProperty(name, std::format("{:.0f}:{:.0f}", property.x, property.y));
	}

	void PushProperty(std::string_view name, const std::string& property) {
		Assertm(cursorObject_, "PushObject() should be called first");
		Assert(!name.empty());
		cursorObject_->PushProperty(name, property);
	}

	template<typename Enum> 
		requires std::is_enum_v<Enum>
	void PushProperty(std::string_view name, Enum property) {
		Assertm(cursorObject_, "PushObject() should be called first");
		Assert(!name.empty());
		cursorObject_->PushProperty(name, ToString(property));
	}

	template<typename T> 
		requires std::is_arithmetic_v<T>
	void PushProperty(std::string_view name, T property) {
		Assertm(cursorObject_, "PushObject() should be called first");
		Assert(!name.empty());

		if constexpr(std::is_integral_v<T>) {
			cursorObject_->PushProperty(name, std::format("{}", property));
		} else if constexpr(std::is_same_v<T, bool>) {
			cursorObject_->PushProperty(name, property ? "True" : "False");
		} else {
			cursorObject_->PushProperty(name, std::format("{:.2f}", property));
		}
	}

public:
	std::list<std::unique_ptr<Object>>	rootObjects_;
	Object*								cursorObject_ = nullptr;
};


// Custom dynamic cast and weak pointer functions
#define WIDGET_CLASS(className, superClassName)\
	DEFINE_CLASS_META(className, superClassName)\
	WeakPtr<className>			GetWeak() {\
		return superClassName::GetWeakAs<className>();\
	}


namespace UI {

	class Object;
	class Widget;
	class Style;
	class LayoutWidget;
	class StatefulWidget;
	class Theme;
	class Tooltip;

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
	//constexpr AxisIndex ToAxisIndex(Axis axis) { assert(axis != Axis::Both); return axis == Axis::X ? AxisX : AxisY; }
	//constexpr Axis ToAxis(int axisIndex) { assert(axisIndex == 0 || axisIndex == 1); return axisIndex ? Axis::Y : Axis::X; }

	// Represent invalid point. I.e. null object for poins
	constexpr auto NOPOINT = Point(std::numeric_limits<float>::max());

	// Used by Flexbox and Aligned widgets
	enum class Alignment {
		Start,
		End,
		Center
	};
	DEFINE_ENUM_TOSTRING_3(Alignment, Start, End, Center)

	enum class EventCategory {
		Debug, 		// DebugLog
		System, 	// Draw, MouseMove, MouseButton, KeyboardKey
		Layout, 	// HitTest, ParentLayout, ChildLayout, Hover
		Callback,
		Notification
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

		static Application* 	Create(std::string_view windowTitle, u32 width, u32 height);
		static Application* 	Get();
		static FrameState		GetState();

		virtual void			Shutdown() = 0;
		// For now we do all ui here
		virtual bool			Tick() = 0;
		// Parents a widget to the root of the widget tree
		// Widgets parented to the root behave like windows and can be reordered
		virtual void			Parent(std::unique_ptr<Widget>&& widget, Layer layer = Layer::Float) = 0;
		virtual std::unique_ptr<Widget>
								Orphan(Widget* widget) = 0;
		virtual void			BringToFront(Widget* widget) = 0;

		virtual Theme* 			GetTheme() = 0;
		// Merges theme styles with the prevous theme overriding existent styles 
		virtual void			SetTheme(Theme* theme) = 0;

		virtual TimerHandle		AddTimer(Object* object, const TimerCallback& callback, u64 periodMs) = 0;
		virtual void			RemoveTimer(TimerHandle handle) = 0;

		virtual void 			RequestRebuild(StatefulWidget* widget) = 0;
	};


	// Custom dynamic cast
	#define EVENT_CLASS(className, superClassName) DEFINE_CLASS_META(className, superClassName)

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

	/*
	* Events dispatched up the tree
	*/
	class Notification: public IEvent {
		EVENT_CLASS(Notification, IEvent)
	public:
		virtual EventCategory GetCategory() const override { return EventCategory::Notification; }

		u32 depth = 0;
	};

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
	class LayoutConstraints final: public IEvent {
		EVENT_CLASS(LayoutConstraints, IEvent)
	public:

		LayoutConstraints()
			: parent(nullptr)
		{}

		LayoutConstraints(LayoutWidget*	parent, Rect rect)
			: parent(parent)
			, rect(rect)
		{}

		EventCategory GetCategory() const override { return EventCategory::Layout; }

		// TODO: Deprecated
		LayoutWidget* parent;
		Rect rect;
	};

	class LayoutNotification final: public Notification {
		EVENT_CLASS(LayoutNotification, Notification)
	public:
		LayoutWidget* source = nullptr;
		Rect          rectLocal;
	};

	/*
	* TODO: deprecated
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

		ChildLayoutEvent(LayoutWidget* child,
				   float2 size, 
				   Vec2<bool> axisChanged = {true, true}, 
				   Subtype type = OnChanged)
			: subtype(type)
			, bAxisChanged(axisChanged)
			, size(size)
			, child(child)
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
		constexpr void Push(LayoutWidget* widget, Point posLocal);

		constexpr bool Empty() const { return stack.empty(); }

		constexpr HitData& Top() { return stack.back(); }
		constexpr LayoutWidget*  TopWidget() { return stack.empty() ? nullptr : stack.back().widget.GetChecked(); }
		
		// Find hit data for a specified widget
		constexpr const HitData* Find(const LayoutWidget* widget) const {
			for(const auto& hitData : stack) {
				if(hitData.widget == widget) 
					return &hitData;
			}
			return nullptr;
		}
		constexpr bool Contains(const LayoutWidget* widget) const {
			return Find(widget) != nullptr;
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

		// internalPos is a position inside the widget rect relative to origin
		constexpr void PushItem(LayoutWidget* widget, Point internalPos) {
			hitStack.Push(widget, internalPos);
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
		// Number of items hovered before
		u32 depth = 0;
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

		MouseButton	button = MouseButton::None;
		bool		bPressed = false;
		Point		mousePosGlobal;
		Point		mousePosLocal;
	};




	class Canvas {
	public:

		virtual ~Canvas() = default;
		virtual void DrawBox(Rect rect, const BoxStyle* style) = 0;
		virtual void DrawRect(Rect rect, Color color, bool bFilled = true) = 0;

		virtual void DrawText(Point origin, const TextStyle* style, std::string_view textView) = 0;
		virtual void ClipRect(Rect clipRect) = 0;
	};

	class DrawEvent final: public IEvent {
		EVENT_CLASS(DrawEvent, IEvent)
	public:

		bool IsBroadcast() const override { return true; };
		EventCategory GetCategory() const override { return EventCategory::System; }

		Canvas*	canvas = nullptr;
		Theme*	theme = nullptr;
	};






	struct AxisMode {

		enum Mode {
			Fixed,
			Expand, 
			Shrink
		};

		constexpr Mode operator[] (Axis axis) const {
			return axis == Axis::X ? x : y;
		}

		Mode x;
		Mode y;
	};

	constexpr auto axisModeFixed = AxisMode(AxisMode::Fixed, AxisMode::Fixed);
	constexpr auto axisModeExpand = AxisMode(AxisMode::Expand, AxisMode::Expand);
	constexpr auto axisModeShrink = AxisMode(AxisMode::Shrink, AxisMode::Shrink);

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
	* Base class for Widgets and WidgetStates
	* Provides not owning Weak pointers to this
	* Provides custom dynamic cast
	*/
	class Object {
		DEFINE_ROOT_CLASS_META(Object)
	public:

		Object() = default;	
		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;

		virtual ~Object() { if(refCounter_) { refCounter_->OnDestructed(); } }

		template<class T>
		WeakPtr<T> GetWeakAs() {
			Assertf(this->As<T>(), "Cannot cast {} to the class {}", GetDebugID(), T::GetStaticClassName());
			if(!refCounter_) {
				refCounter_ = new util::RefCounter();
			}
			return WeakPtr<T>{this->As<T>(), refCounter_};
		}

		WeakPtr<Object> GetWeak() { return GetWeakAs<Object>(); }
		
		// Returns a debug identifier of this object
		// [MyWidgetClassName: <4 bytes of adress> "MyObjectID"]
		std::string GetDebugID() const; 

	private:
		// Used for custom WeakPtr to this
		util::RefCounter* refCounter_ = nullptr;
	};



	/*
	* Base class for ui components
	*/
	class Widget: public Object {
		WIDGET_CLASS(Widget, Object)
	public:

		// Notify parents that we are about to be destructed
		virtual void Destroy() {
			std::unique_ptr<Widget> self = GetParent()->Orphan(this);
			// Destructed here
		}
		
		// Calls a visitor callback on this object and children if bRecursive == true
		virtual VisitResult	VisitChildren(const WidgetVisitor& visitor, bool bRecursive = false) { return visitResultContinue; }
		VisitResult VisitChildrenRecursively(const WidgetVisitor& visitor) { return VisitChildren(visitor, true); }

		// Visits parents in the tree recursively
		void VisitParent(const WidgetVisitor& visitor, bool bRecursive = true) {
			for(auto parent = parent_; parent; parent = parent->GetParent()) {
				const auto result = visitor(parent);
				if(!result.bContinue || !bRecursive) return;
			}
		}

		// Attaches this widget to a parent slot
		virtual void Parent(std::unique_ptr<Widget>&& widget) { Assertf(false, "Class {} cannot have children.", GetClassName()); }
		NODISCARD virtual std::unique_ptr<Widget> Orphan(Widget* widget) { Assertf(false, "Class {} cannot have children.", GetClassName()); return {}; }

		virtual void OnParented(Widget* parent) { parent_ = parent; };
		virtual void OnOrphaned() { parent_ = nullptr; }
			 
		virtual bool OnEvent(IEvent* event) {	
			if(auto* log = event->As<DebugLogEvent>()) {
				log->archive->PushObject(GetClassName().data(), this, GetParent());
				DebugSerialize(*log->archive);
				return true;
			}
			return false;
		};

		virtual void DebugSerialize(PropertyArchive& archive) {}

		// TODO: Move to LayoutWidget
		// Transforms a point in local space into a point in global space
		// recursively iterating parents
		virtual Point TransformLocalToGlobal(Point position) const { 
			return parent_ ? parent_->TransformLocalToGlobal(position) : position; 
		}

		bool DispatchToParent(IEvent* event) { if(parent_) { return parent_->OnEvent(event); } return false; }		

		void DispatchNotification(Notification* notification) {
			for(auto* ancestor = parent_; ancestor; ancestor = ancestor->GetParent()) {
				auto handled = ancestor->OnEvent(notification);
				if(handled) {
					++notification->depth;
				}
			}
		}

	public:

		StringID GetID() const { return id_; }
		void SetID(StringID id) { id_ = id; }

		Widget* GetParent() { return parent_; }
		const Widget* GetParent() const { return parent_; }

		// Gets nearest parent in the tree of the class T
		template<WidgetSubclass T>
		const T* FindParentOfClass() const {
			for(auto* parent = GetParent(); parent; parent = parent->GetParent()) {
				if(parent->IsA<T>()) 
					return static_cast<const T*>(parent);
			}
			return nullptr;
		}

		template<WidgetSubclass T>
		T* FindParentOfClass() { return const_cast<T*>(std::as_const(*this).FindParentOfClass<T>()); }

		// Looks for a child with specified class
		// @param bRecursive - Looks among children of children
		template<WidgetSubclass T>
		T* FindChildOfClass(bool bRecursive = true) {
			T* out = nullptr;
			VisitChildren([&](Widget* child) {
				if(child->IsA<T>()) {
					out = static_cast<T*>(child);
					return visitResultExit; 
				}
				return visitResultContinue;
			},
			bRecursive);
			return out;
		}

	protected:
		Widget(const std::string& id = {})
			: id_(id)
			, parent_(nullptr)
		{}

	private:
		// Optional user defined ID
		StringID			id_;
		// Our parent widget who manages our lifetime and passes events
		Widget*				parent_;
	};
	

	/*
	* Abstract widget container that has one child
	*/
	class SingleChildWidget: public Widget {
		WIDGET_CLASS(SingleChildWidget, Widget)
	public:

		void Parent(std::unique_ptr<Widget>&& widget) override {
			Assert(widget && !widget->GetParent() && !child_);
			child_ = std::move(widget);
			child_->OnParented(this);
		}

		NODISCARD std::unique_ptr<Widget> Orphan(Widget* widget) override {
			Assert(widget);
			if(child_.get() == widget) {
				child_->OnOrphaned();
				return std::move(child_);
			}
			return {};
		}
		NODISCARD std::unique_ptr<Widget> OrphanChild() { return Orphan(GetChild()); }

		bool DispatchToChildren(IEvent* event) {
			if(child_) {
				return child_->OnEvent(event);
			}
			return false;
		}

		VisitResult VisitChildren(const WidgetVisitor& visitor, bool bRecursive = true) final {
			if(!child_) return visitResultContinue;

			const auto result = visitor(child_.get());
			if(!result.bContinue) return visitResultExit;

			if(bRecursive && !result.bSkipChildren) {
				const auto result = child_->VisitChildren(visitor, bRecursive);
				if(!result.bContinue) return visitResultExit;
			}
			return visitResultContinue;
		}
		
		bool OnEvent(IEvent* event) override {
			if(auto* log = event->As<DebugLogEvent>()) {
				log->archive->PushObject(GetDebugID(), this, GetParent());
				DebugSerialize(*log->archive);
				DispatchToChildren(event);
				return true;
			} 
			return Super::OnEvent(event);
		};

		Widget* GetChild() { return child_.get(); }

	protected:
		SingleChildWidget(const std::string& id = {})
			: Widget(id)
			, child_()
		{}

	private:
		std::unique_ptr<Widget> child_;
	};


	class StatefulWidget;

	/*
	*/
	class WidgetState: public Object {
		WIDGET_CLASS(WidgetState, Object)
	public:
		friend class StatefulWidget;

		WidgetState() = default;
		~WidgetState();

		WidgetState(const WidgetState&) = delete;
		WidgetState& operator=(const WidgetState&) = delete;

		virtual std::unique_ptr<Widget> Build() = 0;

		void MarkNeedsRebuild();

		void SetVisibility(bool isVisible) {
			isVisible_ = isVisible;
			MarkNeedsRebuild();
		}

		bool IsVisible() { return isVisible_; }

		// Called after build
		// Could be null if the widget tree is destructed before the state
		void SetWidget(StatefulWidget* widget) {
			widget_ = widget;
			// If our widget has been unmounted and destroyes mark us dirty
			// so we will be built at the next mount
			if(!widget_) {
				needsRebuild_ = true;
			}
		}

		StatefulWidget* GetWidget() { return widget_; }

	private:
		bool isVisible_ = true;
		bool needsRebuild_ = true;
		StatefulWidget* widget_ = nullptr;
	};


	/*
	* Owns widgets build by the bound state but the state 
	* is owned by the user nad managed externally, and when the user changes the state
	* it triggers rebuild, and a result of the rebuild is parented to this widget
	*/
	class StatefulWidget: public SingleChildWidget {
		WIDGET_CLASS(StatefulWidget, SingleChildWidget)
	public:

		static std::unique_ptr<StatefulWidget> New(WidgetState* state) {
			return std::make_unique<StatefulWidget>(state);
		}

		StatefulWidget(WidgetState* state)
			: state_(state)
		{}
		
		~StatefulWidget() {
			if(state_) {
				state_->SetWidget(nullptr);
			}
		}

		void MarkNeedsRebuild() {
			state_->needsRebuild_ = true;
			Application::Get()->RequestRebuild(this);
		}

		std::unique_ptr<Widget> Build() {
			Assertf(state_, "{} doesn't have a state.", GetDebugID());
			state_->needsRebuild_ = false;
			return state_->Build();
		}

		bool NeedsRebuild() const {
			Assertf(state_, "{} doesn't have a state.", GetDebugID());
			return state_->needsRebuild_;
		}

		// Bind this widget to the state and unbind the old one
		void RebindToState() {
			Assertf(state_, "{} doesn't have a state.", GetDebugID());
			auto* old = state_->GetWidget();
			state_->SetWidget(this);

			if(old && old != this) {
				old->state_ = nullptr;
			}
		}

		WidgetState* GetState() { return state_; }

	private:
		WidgetState* state_;
	};

	inline void WidgetState::MarkNeedsRebuild() { 
		Assertf(widget_, "State doesn't have a widget.");
		widget_->MarkNeedsRebuild();
	}

	inline WidgetState::~WidgetState() {
		if(widget_) {
			widget_->Destroy();
		}
	}


	/*
	* A widget which has size and position
	* And possibly can be drawn
	*/
	class LayoutWidget: public Widget {
		WIDGET_CLASS(LayoutWidget, Widget)
	public:

		bool OnEvent(IEvent* event) override {
			if(auto* hitTest = event->As<HitTestEvent>()) {
				if(!IsVisible()) return false;
				auto hitPosLocalSpace = hitTest->GetLastHitPos();

				if(GetRect().Contains(hitPosLocalSpace)) {
					hitTest->PushItem(this, hitPosLocalSpace - GetOrigin());
					return true;
				}
				return false;
			} 
			return Super::OnEvent(event);
		};

		void DebugSerialize(PropertyArchive& archive) override {
			Super::DebugSerialize(archive);
			archive.PushProperty("IsVisible", !bHidden_);
			archive.PushProperty("IsFloatLayout", (bool)bFloatLayout_);
			archive.PushProperty("AxisMode", std::format("{}", axisMode_));
			archive.PushProperty("Origin", origin_);
			archive.PushProperty("Size", size_);
		}

		Point TransformLocalToGlobal(Point localPos) const override {
			return GetParent() ? GetParent()->TransformLocalToGlobal(localPos + origin_) : localPos + origin_;
		}

	public:

		// Looks for a nearest LayoutWidget down the tree starting from widget
		static LayoutWidget* FindNearest(Widget* widget) {
			if(widget->IsA<LayoutWidget>())
				return widget->As<LayoutWidget>();
			return widget->FindChildOfClass<LayoutWidget>();
		}

		// Finds the topmost widget up the tree that is not another layout widget
		// Because LayoutWidget serves as a base for widgets composition
		// Other widgets that wrap this widget will use it for hovering, clicking, moving behaviour
		Widget*	FindTopmost() {
			Widget* topmost = this;

			for(auto* parent = GetParent();
				parent && !parent->IsA<LayoutWidget>();
				parent = parent->GetParent()) {
				topmost = parent;
			}
			return topmost;
		}

		AxisMode GetAxisMode() const { return axisMode_; }

		void SetAxisMode(AxisMode mode) { axisMode_ = mode; }

		void SetAxisMode(Axis axis, AxisMode::Mode mode) {
			if(axis == Axis::X) {
				axisMode_.x = mode;
			} else if(axis == Axis::Y) {
				axisMode_.y = mode;
			}
		}

		// Hiddent objects won't draw themselves and won't handle hovering 
		// but layout update should be managed by the parent
		void SetVisibility(bool bVisible) { bHidden_ = !bVisible; }
		bool IsVisible() const { return !bHidden_; }

		// Should be called by subclasses
		void SetOrigin(Point pos) { origin_ = pos; }
		void SetOrigin(float inX, float inY) { origin_ = {inX, inY}; }
		void SetOrigin(Axis axis, float pos) { origin_[axis] = pos; }

		void Translate(float2 offset) { origin_ += offset; }
		void Translate(float inX, float inY) { origin_.x += inX; origin_.y += inY; }

		void SetSize(float2 size) { size_ = size; }
		void SetSize(Axis axis, float size) { size_[(int)axis] = size; }
		void SetSize(float inX, float inY) { size_ = {inX, inY}; }

		Rect GetRect() const { return {origin_, size_}; }
		// Returns Rect of this widget in global coordinates
		Rect GetRectGlobal() const { return Rect(GetParent() ? GetParent()->TransformLocalToGlobal(origin_) : origin_, size_); }
		float2 GetSize() const { return size_; }
		float GetSize(Axis axis) const { return size_[axis]; }

		Point GetOrigin() const { return origin_; }
		Point GetTransform() const { return origin_; }

		// Size + Margins
		float2 GetOuterSize() {
			const auto size = GetSize();
			const auto margins = GetLayoutStyle() ? GetLayoutStyle()->margins : Margins{};
			return {size.x + margins.Left + margins.Right, size.y + margins.Top + margins.Bottom};
		}

		// Size - Paddings
		float2 GetInnerSize() {
			const auto size = GetSize();
			const auto paddings = GetLayoutStyle() ? GetLayoutStyle()->paddings : Paddings{};
			return {size.x - paddings.Left - paddings.Right, size.y - paddings.Top - paddings.Bottom};
		}

		void SetLayoutStyle(const LayoutStyle* style) { layoutStyle_ = style; }
		const LayoutStyle* GetLayoutStyle() const { return layoutStyle_; }

		// Widget's position won't be affected by parent's layout events
		void SetFloatLayout(bool bEnable = true) { bFloatLayout_ = bEnable; }
		bool IsFloatLayout() const { return bFloatLayout_; }
	
	public:

		// Returns size after layout is performed
		virtual float2 OnLayout(const LayoutConstraints& event) { 
			const auto margins = GetLayoutStyle()->margins;

			if(!bFloatLayout_) {
				SetOrigin(event.rect.TL() + margins.TL());
			}
			for(auto axis: Axes2D) {
				if(GetAxisMode()[axis] == AxisMode::Expand) {
					SetSize(axis, event.rect.Size()[axis] - margins.Size()[axis]);
				}
			}
			return GetOuterSize();
		}

		// Should be called by subclasses at the end of their OnLayout()
		void OnPostLayout() {
			if(bNotifyOnUpdate_) {
				LayoutNotification e;
				e.rectLocal = GetRect();
				e.source = this;
				DispatchNotification(&e);
			}
		}

	protected:
		LayoutWidget(const LayoutStyle* style = nullptr,
					 AxisMode			axisMode = axisModeShrink,
					 bool 				notifyOnUpdate = false,
					 const std::string& id = {})
			: Widget(id) 
			, bHidden_(false)
			, bFloatLayout_(false)
			, bNotifyOnUpdate_(notifyOnUpdate)
			, axisMode_(axisModeFixed)
			, layoutStyle_(style) {
			SetAxisMode(axisMode);
		}

	private:
		u8					bHidden_:1;
		u8					bFloatLayout_:1;
		// Send notifications to ancestors when updated
		u8					bNotifyOnUpdate_:1;
		AxisMode			axisMode_;
		// Position in pixels relative to parent origin
		Point				origin_;
		float2				size_;
		// Style object in the Theme
		const LayoutStyle*	layoutStyle_;
	};


	/*
	* LayoutWidget container that has one child
	*/
	class SingleChildLayoutWidget: public LayoutWidget {
		WIDGET_CLASS(SingleChildLayoutWidget, LayoutWidget)
	public:		

		void Parent(std::unique_ptr<Widget>&& widget) override {
			Assert(widget && !widget->GetParent() && !child_);
			child_ = std::move(widget);
			child_->OnParented(this);
			DispatchLayoutToChildren();
		}

		NODISCARD std::unique_ptr<Widget> Orphan(Widget* widget) override {
			Assert(widget);
			if(child_.get() == widget) {
				child_->OnOrphaned();
				return std::move(child_);
			}
			return {};
		}

		bool DispatchToChildren(IEvent* event) {
			if(child_) {
				return child_->OnEvent(event);
			}
			return false;
		}

		VisitResult VisitChildren(const WidgetVisitor& visitor, bool bRecursive = true) final {
			if(!child_) return visitResultContinue;

			const auto result = visitor(child_.get());
			if(!result.bContinue) return visitResultExit;

			if(bRecursive && !result.bSkipChildren) {
				const auto result = child_->VisitChildren(visitor, bRecursive);
				if(!result.bContinue) return visitResultExit;
			}
			return visitResultContinue;
		}

		bool OnEvent(IEvent* event) override {
			if(auto* log = event->As<DebugLogEvent>()) {
				log->archive->PushObject(GetDebugID(), this, GetParent());
				DebugSerialize(*log->archive);
				DispatchToChildren(event);
				return true;
			} 
			if(event->IsA<HitTestEvent>()) {
				if(Super::OnEvent(event)) {
					// Dispatch HitTest only if we was hit
					if(auto* layoutChild = FindChildOfClass<LayoutWidget>(this)) {
						layoutChild->OnEvent(event);
					}
					return true;
				} 
				return false;
			} 
			return Super::OnEvent(event);
		}

	public:

		float2 OnLayout(const LayoutConstraints& rect) override { 
			const auto layoutInfo = *GetLayoutStyle();
			const auto margins = layoutInfo.margins;

			if(!IsFloatLayout()) {
				SetOrigin(rect.rect.TL() + margins.TL());
			}
			for(auto axis: Axes2D) {
				if(GetAxisMode()[axis] == AxisMode::Expand) {
					SetSize(axis, rect.rect.Size()[axis] - margins.Size()[axis]);
				}
			}
			if(!child_) {
				return GetSize() + margins.Size();
			}
			LayoutConstraints e;
			e.parent = this;
			e.rect = Rect(layoutInfo.paddings.TL(), GetSize() - layoutInfo.paddings.Size());

			if(auto* layoutWidget = FindChildOfClass<LayoutWidget>()) {
				const auto childOuterSize = layoutWidget->OnLayout(e);
				layoutWidget->OnPostLayout();

				for(auto axis: Axes2D) {
					if(GetAxisMode()[axis] == AxisMode::Shrink) {
						SetSize(axis, childOuterSize[axis] + layoutInfo.paddings.Size()[axis]);
					}
				}
			}
			return GetSize() + margins.Size();
		}

	protected:

		// Centers our child when child size changes
		void CenterChild(ChildLayoutEvent* event) {
			const auto childSize = event->child->GetSize();
			const auto position = (Super::GetSize() - childSize) * 0.5f;
			event->child->SetOrigin(position);
		}

		// Centers our child when parent layout changes
		void CenterChild(LayoutConstraints* event) {
			if(auto* child = Super::FindChildOfClass<LayoutWidget>()) {
				const auto margins = GetLayoutStyle() ? GetLayoutStyle()->margins : Margins{};
				const auto childOuterSize = child->GetSize() + margins.Size();
				const auto float2 = (event->rect.Size() - childOuterSize) * 0.5f;
				// Pass tight rect to force child position
				LayoutConstraints onParent(this, Rect(float2, childOuterSize));
				child->OnEvent(&onParent);
			}
		}
		// Dispatches event to children adding paddings to rect
		void DispatchLayoutToChildren() {
			if(!child_) return;

			const auto layoutInfo = GetLayoutStyle();
			LayoutConstraints layoutEvent;
			layoutEvent.parent = this;
			layoutEvent.rect = Rect(layoutInfo->paddings.TL(), GetSize() - layoutInfo->paddings.Size());

			if(auto* layoutWidget = FindChildOfClass<LayoutWidget>()) {
				//LOGF(Verbose, "Widget {} dispatched parent layout event to child {}", GetDebugID(), layoutWidget->GetDebugID());
				layoutWidget->OnEvent(&layoutEvent);
			}
		}

		/**
		 * TODO: deprecated
		 * Helper to update our size based on AxisMode
		 * @param bUpdateChildOnAdded if true, updates a newly added child
		 * @return true if our size was changed by the child
		*/
		bool HandleChildEvent(const ChildLayoutEvent* event, bool bUpdateChildOnAdded = true) {
			const auto axisMode = GetAxisMode();
			const auto prevSize = GetSize();
			const auto childAxisMode = event->child->GetAxisMode();
			const auto paddings = GetLayoutStyle() ? GetLayoutStyle()->paddings : Paddings{};
			const auto paddingsSize = paddings.Size();
			bool bSizeChanged = false;

			if(event->subtype == ChildLayoutEvent::OnRemoved) {
				// Default behavior
				SetSize({0.f, 0.f});
				bSizeChanged = true;

			} else if(event->subtype == ChildLayoutEvent::OnChanged) {
				for(auto axis: Axes2D) {
					if(event->bAxisChanged[axis] && axisMode[axis] == AxisMode::Shrink) {
						Assertf(childAxisMode[axis] != AxisMode::Expand,
								"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked."
								"Child {}, Parent {}", event->child->GetDebugID(), GetDebugID());

						SetSize(axis, event->size[axis] + paddingsSize[axis]);
						bSizeChanged = prevSize[axis] != GetSize()[axis];
					}
				}

			} else if(event->subtype == ChildLayoutEvent::OnAdded) {
				for(auto axis: Axes2D) {
					if(axisMode[axis] == AxisMode::Shrink) {
						Assertf(childAxisMode[axis] != AxisMode::Expand,
								"Cannot shrink on a child with expand behavior. Parent and child behaviour should match if parent is shrinked. "
								"Child: {}, Parent: {}", event->child->GetDebugID(), GetDebugID());

						SetSize(axis, event->size[axis] + paddingsSize[axis]);
						bSizeChanged = prevSize[axis] != GetSize()[axis];
					}
				}
				// Update child's position based on padding
				if(bUpdateChildOnAdded) {
					LayoutConstraints onParent;
					onParent.rect = Rect(paddings.TL(), GetSize() - paddingsSize);
					event->child->OnEvent(&onParent);
				}
			}
			if(bSizeChanged){
				//LOGF(Verbose, "{} has been updated on child event. New size: {}", GetDebugID(), GetSize());
			}
			return bSizeChanged;
		}

	protected:	
		SingleChildLayoutWidget(StringID 			styleName,
								AxisMode			axisMode = axisModeShrink,
								bool				notifyOnUpdate = false,
								const std::string&	id = {})
			: LayoutWidget(Application::Get()->GetTheme()->Find(styleName)->FindOrDefault<LayoutStyle>(), 
						   axisMode, 
						   notifyOnUpdate,
						   id)
			, child_()
		{}
	
	public:
		std::unique_ptr<Widget> child_;
	};



	/*
	* LayoutWidget container that has multiple children
	*/
	class MultiChildLayoutWidget: public LayoutWidget {
		WIDGET_CLASS(MultiChildLayoutWidget, LayoutWidget)
	public:

		void Parent(std::unique_ptr<Widget>&& widget) override {
			Assert(widget && !widget->GetParent());
			auto* w = widget.get();
			children_.push_back(std::move(widget));
			w->OnParented(this);
		}

		std::unique_ptr<Widget> Orphan(Widget* widget) override {
			Assert(widget);
			for(auto it = children_.begin(); it != children_.end(); ++it) {
				if(it->get() == widget) {
					auto out = std::move(*it);
					children_.erase(it);
					widget->OnOrphaned();
					return out; 
				}
			}			
			return {};
		}

		bool DispatchToChildren(IEvent* event) {
			for(auto& child : children_) {
				auto result = child->OnEvent(event);

				if(result && !event->IsBroadcast()) {
					return result;
				}
			}
			return false;
		}

		void GetVisibleChildren(std::vector<LayoutWidget*>* outVisibleChildren) {
			outVisibleChildren->reserve(children_.size());

			for(auto& child : children_) {
				auto layoutChild = child->As<LayoutWidget>();

				if(!layoutChild) {
					layoutChild = child->FindChildOfClass<LayoutWidget>();
				}

				if(layoutChild && layoutChild->IsVisible()) {
					outVisibleChildren->push_back(layoutChild);
				}
			}
		}

		VisitResult VisitChildren(const WidgetVisitor& visitor, bool bRecursive = false) final {
			for(auto& child : children_) {
				const auto result = visitor(child.get());
				if(!result.bContinue) return visitResultExit;

				if(bRecursive && !result.bSkipChildren) {
					const auto result = child->VisitChildren(visitor, bRecursive);
					if(!result.bContinue) return visitResultExit;
				}
			}
			return visitResultContinue;
		}

		bool OnEvent(IEvent* event) override {
			if(event->IsA<HitTestEvent>()) {
				if(Super::OnEvent(event)) {
					for(auto& child: children_) {
						auto* layoutChild = LayoutWidget::FindNearest(child.get());
						if(layoutChild && layoutChild->OnEvent(event)) {
							break;
						}
					}
					return true;
				} 
				return false;
			} 
			if(auto* log = event->As<DebugLogEvent>()) {
				log->archive->PushObject(GetDebugID(), this, GetParent());
				DebugSerialize(*log->archive);
				DispatchToChildren(event);
				return true;
			} 
			return Super::OnEvent(event);
		}

	protected:
		MultiChildLayoutWidget(const LayoutStyle* style = nullptr,
							   AxisMode 		  axisMode = axisModeShrink,
							   bool				  notifyOnUpdate = false,
							   const std::string& id = {})
			: LayoutWidget(style, axisMode, notifyOnUpdate, id)
			, children_() 
		{}

	private:
		std::vector<std::unique_ptr<Widget>> children_;
	};




	class MouseRegionBuilder;

	using MouseEnterEventCallback = std::function<void()>;
	using MouseLeaveEventCallback = std::function<void()>;
	using MouseHoverEventCallback = std::function<void(const HoverEvent&)>;
	using MouseButtonEventCallback = std::function<void(const MouseButtonEvent&)>;
	using MouseDragEventCallback = std::function<void(const MouseDragEvent&)>;

	struct MouseRegionConfig {
		MouseEnterEventCallback  onMouseEnter;
		MouseLeaveEventCallback  onMouseLeave;
		MouseHoverEventCallback  onMouseHover;
		MouseButtonEventCallback onMouseButton;
		MouseDragEventCallback   onMouseDrag;
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

		bool OnEvent(IEvent* event) override {
			if(auto* hoverEvent = event->As<HoverEvent>()) {
				if(hoverEvent->bHoverEnter) {
					if(config_.onMouseEnter) {
						config_.onMouseEnter();
					} else if(config_.onMouseHover) {
						config_.onMouseHover(*hoverEvent);
					}
				} else if(hoverEvent->bHoverLeave) {
					if(config_.onMouseLeave) {
						config_.onMouseLeave();
					}
				} else if(config_.onMouseHover) {
					config_.onMouseHover(*hoverEvent);
				}
				return config_.onMouseEnter || config_.onMouseHover;
			}
			if(auto* dragEvent = event->As<MouseDragEvent>()) {
				if(config_.onMouseDrag) {
					config_.onMouseDrag(*dragEvent);
					return true;
				}
				return config_.onMouseDrag || config_.onMouseButton;
			}
			if(auto* buttonEvent = event->As<MouseButtonEvent>()) {
				if(config_.onMouseButton) {
					config_.onMouseButton(*buttonEvent);
					return true;
				}
				return config_.onMouseDrag || config_.onMouseButton;
			}
			return Super::OnEvent(event);
		}

		bool ShouldAlwaysReceiveHover() const { return config_.bHandleHoverAlways; }
		bool ShouldAlwaysReceiveButton() const { return config_.bHandleButtonAlways; }

	protected:
		MouseRegion(const MouseRegionConfig& config) 
			: config_(config)
		{}
		friend class MouseRegionBuilder;
	
	private:	
		MouseRegionConfig config_;
	};

	class MouseRegionBuilder {
	public:

		MouseRegionBuilder& OnMouseButton(const MouseButtonEventCallback& c) { config_.onMouseButton = c; return *this; }
		MouseRegionBuilder& OnMouseDrag(const MouseDragEventCallback& c) { config_.onMouseDrag = c; return *this; }
		MouseRegionBuilder& OnMouseEnter(const MouseEnterEventCallback& c) { config_.onMouseEnter = c; return *this; }
		MouseRegionBuilder& OnMouseLeave(const MouseLeaveEventCallback& c) { config_.onMouseLeave = c; return *this; }
		MouseRegionBuilder& OnMouseHover(const MouseHoverEventCallback& c) { config_.onMouseHover = c; return *this; }
		MouseRegionBuilder& HandleHoverAlways(bool b = true) { config_.bHandleHoverAlways = b; return *this; }
		MouseRegionBuilder& HandleButtonAlways(bool b = true) { config_.bHandleButtonAlways = b; return *this; }
		MouseRegionBuilder& Child(std::unique_ptr<Widget>&& child) { child_ = std::move(child); return *this; }

		std::unique_ptr<MouseRegion> New() { 
			Assertf([&]() {
				if(config_.onMouseHover || config_.onMouseEnter) {
					return (bool)config_.onMouseLeave;
				}
				return true;
			}(), "OnMouseLeave callback should be set if OnMouseEnter or OnMouseHover is set");

			std::unique_ptr<MouseRegion> out(new MouseRegion(config_));
			out->Parent(std::move(child_));
			return out;
		}

	private:
		MouseRegionConfig       config_;
		std::unique_ptr<Widget> child_;
	};

	inline MouseRegionBuilder MouseRegion::Build() { return {}; }







	using EventCallback = std::function<bool(IEvent*)>;

	/*
	* TODO: maybe rename into NotificationListener and separate notificatoins and events
	* Calls the user provided callback when an event is received
	*/
	class EventListener: public SingleChildWidget {
		WIDGET_CLASS(EventListener, SingleChildWidget)
	public:

		static auto New(const EventCallback& callback, std::unique_ptr<Widget>&& child) {
			auto out = std::make_unique<EventListener>();
			out->callback_ = callback;
			out->Parent(std::move(child));
			return out;
		}

		bool OnEvent(IEvent* event) override {
			if(callback_) {
				return callback_(event);
			}
			return false;
		}

	private:
		EventCallback callback_;
	};


}

constexpr void UI::HitStack::Push(LayoutWidget* widget, Point posLocal) {
	stack.push_back(HitData{widget->GetWeak(), posLocal});
}

template<>
struct std::formatter<UI::AxisMode, char> {

	template<class ParseContext>
	constexpr ParseContext::iterator parse(ParseContext& ctx) {
		return ctx.begin();
	}

	template<class FmtContext>
	FmtContext::iterator format(const UI::AxisMode& axisMode, FmtContext& ctx) const {
		const auto out = std::format("({}:{})", 
			axisMode.x == UI::AxisMode::Expand ? "Expand" : axisMode.x == UI::AxisMode::Shrink ? "Shrink" : "Fixed",
			axisMode.y == UI::AxisMode::Expand ? "Expand" : axisMode.y == UI::AxisMode::Shrink ? "Shrink" : "Fixed");
		return std::ranges::copy(std::move(out), ctx.out()).out;
	}
};

inline std::string UI::Object::GetDebugID() const {
	if(auto* w = this->As<Widget>(); !w->GetID().Empty()) {
		return std::format("[{}: {:x} \"{}\"]", GetClassName(), (uintptr_t)this & 0xffff, w->GetID());
	} else {
		return std::format("[{}: {:x}]", GetClassName(), (uintptr_t)this & 0xffff);
	}
}