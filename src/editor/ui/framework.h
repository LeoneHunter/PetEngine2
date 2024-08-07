#pragma once
#include <functional>
#include <ranges>

#include "runtime/common.h"
#include "runtime/util.h"
#include "style.h"

using Point = Vec2<float>;

namespace ui {
class Widget;
}

// Custom dynamic cast and weak pointer functions
#define WIDGET_CLASS(className, superClassName)        \
    DEFINE_CLASS_META(className, superClassName)       \
    WeakPtr<className> GetWeak() {                     \
        return superClassName::GetWeakAs<className>(); \
    }


// TODO: remove indent
namespace ui {

class Object;
class Widget;
class Style;
class LayoutWidget;
class StatefulWidget;
class Theme;
class Tooltip;
class FocusNode;

template <typename T>
concept WidgetSubclass = std::derived_from<T, Widget>;

// User provided function used to spawn other widgets
// Used in a tooltip and dragdrop
using SpawnerFunction = std::function<LayoutWidget*()>;

using Margins = RectSides<uint32_t>;
using Paddings = RectSides<uint32_t>;
using WidgetSlot = size_t;

constexpr auto defaultWidgetSlot = WidgetSlot();

// Used by Flexbox and Aligned widgets
enum class Alignment { Start, End, Center };
DEFINE_ENUM_TOSTRING_3(Alignment, Start, End, Center)

enum class EventCategory {
    Debug,   // DebugLog
    System,  // Draw, MouseMove, MouseButton, KeyboardKey
    Layout,  // HitTest, ParentLayout, ChildLayout, Hover
    Callback,
    Notification
};

// Mask to check pressed buttons
enum class MouseButtonMask : uint8_t {
    None = 0,
    ButtonLeft = 0x1,
    ButtonRight = 0x2,
    ButtonMiddle = 0x4,
    Button4 = 0x8,
    Button5 = 0x10,
};
DEFINE_ENUM_FLAGS_OPERATORS(MouseButtonMask)

enum class MouseButton : uint8_t {
    None,
    ButtonLeft,
    ButtonRight,
    ButtonMiddle,
    Button4,
    Button5,
};

enum class MouseState {
    Default,   // When hovering an empty space or widget without a tooltip
    Tooltip,   // After some time hovering
    Held,      // When pressed and holding
    Dragged,   // When dragged threshold crossed
    DragDrop,  // When DragDrop active
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


enum class AxisMode {
    Fixed,   // Size is fixed and doesn't depend on parent or children's layout
    Expand,  // Size depends on the parent size
    Shrink   // Size depends on children's sizes and layout
};

constexpr std::string to_string(AxisMode value) {
    switch (value) {
        case AxisMode::Fixed:
            return "Fixed";
        case AxisMode::Expand:
            return "Expand";
        case AxisMode::Shrink:
            return "Shrink";
        default:
            DASSERT_M(false, "Enum value out of range");
            return "";
    }
}

// Defines the size dependency for a LayoutWidget
struct SizeMode {
    AxisMode x;
    AxisMode y;

    constexpr SizeMode() : x(AxisMode::Fixed), y(AxisMode::Fixed) {}
    constexpr SizeMode(AxisMode x, AxisMode y) : x(x), y(y) {}

    static constexpr SizeMode Expand() {
        return {AxisMode::Expand, AxisMode::Expand};
    }
    static constexpr SizeMode Fixed() {
        return {AxisMode::Fixed, AxisMode::Fixed};
    }
    static constexpr SizeMode Shrink() {
        return {AxisMode::Shrink, AxisMode::Shrink};
    }

    constexpr AxisMode operator[](Axis axis) const {
        return axis == Axis::X ? x : y;
    }

    constexpr AxisMode& operator[](Axis axis) {
        return axis == Axis::X ? x : y;
    }
};

constexpr bool operator==(const SizeMode& lhs, const SizeMode& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}


/*
 * Some global state information of the application
 * Can be accesed at any time in read only mode
 * Users should not cache this because its valid only for one frame
 */
struct FrameState {
    Point mousePosGlobal;
    // Number of buttons currently held
    uint8_t mouseButtonHeldNum = 0;
    MouseButtonMask mouseButtonsPressedBitField = MouseButtonMask::None;
    // Position of the mouse cursor when the first mouse button has been pressed
    Point mousePosOnCaptureGlobal;
    KeyModifiersArray keyModifiersState{false};
    float2 windowSize;
    Theme* theme = nullptr;
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

    static Application* Create(std::string_view windowTitle,
                               uint32_t width,
                               uint32_t height);
    static Application* Get();
    static FrameState GetState();

    virtual void Shutdown() = 0;
    // For now we do all ui here
    virtual void Run() = 0;
    // Parents a widget to the root of the widget tree
    // Widgets parented to the root behave like windows and can be reordered
    virtual void Parent(std::unique_ptr<Widget>&& widget,
                        Layer layer = Layer::Float) = 0;
    virtual std::unique_ptr<Widget> Orphan(Widget* widget) = 0;
    virtual void BringToFront(Widget* widget) = 0;

    virtual Theme* GetTheme() = 0;

    virtual TimerHandle AddTimer(Object* object,
                                 const TimerCallback& callback,
                                 uint64_t periodMs) = 0;
    virtual void RemoveTimer(TimerHandle handle) = 0;

    virtual void RequestRebuild(StatefulWidget* widget) = 0;
    virtual void RequestFocus(FocusNode* focusNode) = 0;
};


// Custom dynamic cast
#define EVENT_CLASS(className, superClassName) \
    DEFINE_CLASS_META(className, superClassName)

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
    virtual EventCategory GetCategory() const {
        return EventCategory::Callback;
    };

    // Returns a debug identifier of this object
    // [ClassName: <4 bytes of adress>]
    std::string GetDebugID() {
        return std::format("[{}: {:x}]", GetClassName(),
                           (uintptr_t)this & 0xffff);
    }
};

/*
 * Events dispatched up the tree
 */
class Notification : public IEvent {
    EVENT_CLASS(Notification, IEvent)
public:
    virtual EventCategory GetCategory() const override {
        return EventCategory::Notification;
    }

    uint32_t depth = 0;
};

/*
 * Request to draw a widget debug data
 */
class DebugLogEvent final : public IEvent {
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
class MouseDragEvent final : public IEvent {
    EVENT_CLASS(MouseDragEvent, IEvent)
public:
    EventCategory GetCategory() const override { return EventCategory::System; }

    // If during this event mouse button is being held
    // This is the point of initial mouse press
    Point mousePosOnCaptureLocal;
    // Position of the mouse inside the hovered widget
    // Relative to the widget size
    Point mousePosOnCaptureInternal;
    Point mousePosLocal;
    float2 mouseDelta;
    MouseButtonMask mouseButtonsPressedBitField = MouseButtonMask::None;
};

class MouseScrollEvent final : public IEvent {
    EVENT_CLASS(MouseScrollEvent, IEvent)
public:
    EventCategory GetCategory() const override { return EventCategory::System; }

    float2 scrollDelta;
};

/*
 * Passed to children of a widget when it's layout needs update
 * Widgets that are not subclasses of LayoutWidget do not care about these
 * events Constraints define the area in which a child can position itself and
 * change size
 */
struct LayoutConstraints {
    LayoutConstraints() = default;

    LayoutConstraints(LayoutWidget* parent, Rect rect)
        : parent(parent), rect(rect) {}
    LayoutWidget* parent{};
    Rect rect;
};

class LayoutNotification final : public Notification {
    EVENT_CLASS(LayoutNotification, Notification)
public:
    LayoutWidget* source = nullptr;
    Rect rectLocal;
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
        WeakPtr<LayoutWidget> widget;
        Point hitPosLocal;
    };

public:
    constexpr void Push(LayoutWidget* widget, Point posLocal);

    constexpr bool Empty() const { return stack.empty(); }

    constexpr HitData& Top() { return stack.back(); }

    constexpr LayoutWidget* TopWidget() {
        return stack.empty() ? nullptr : stack.back().widget.GetChecked();
    }

    // Find hit data for a specified widget
    constexpr const HitData* Find(const LayoutWidget* widget) const {
        for (const auto& hitData : stack) {
            if (hitData.widget == widget)
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
class HitTestEvent final : public IEvent {
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
    Point hitPosGlobal;
    HitStack hitStack;
};



/*
 * Dispatched to the last widget in the hittest stack
 * If the widget doen't want to handle the event it passes it to the parent
 * The dispatched also responsible for handling hover out events
 */
class HoverEvent final : public IEvent {
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

    bool bHoverEnter = false;
    bool bHoverLeave = false;
    // Number of items hovered before
    uint32_t depth = 0;
};



/*
 * System event dispatched by Application when os window is clicked
 * First its dispatched to the top widget of the hitstack until a widget
 *	that returns true is found. Then [handled] is set to true and event
 *	continues to propagate up the tree so that other widget and wrappers can
 *	handle it differently.
 */
class MouseButtonEvent final : public IEvent {
    EVENT_CLASS(MouseButtonEvent, IEvent)
public:
    EventCategory GetCategory() const override { return EventCategory::System; }

    MouseButton button = MouseButton::None;
    bool isPressed = false;
    Point mousePosGlobal;
    Point mousePosLocal;
};



/*
 * Interface to ImGui::Drawlist
 * Uses global coordinates
 * Manages relative coordinates transformation
 * Allocates space in the vertex buffer and pushes a shape into it
 * That buffer then passed to the Renderer for actual rendering
 */
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void DrawBox(Rect rect, const BoxStyle* style) = 0;
    virtual void DrawRect(Rect rect, Color color, bool bFilled = true) = 0;

    virtual void DrawText(Point origin,
                          const TextStyle* style,
                          std::string_view textView) = 0;
    virtual void ClipRect(Rect clipRect) = 0;
};

class DrawEvent final : public IEvent {
    EVENT_CLASS(DrawEvent, IEvent)
public:
    bool IsBroadcast() const override { return true; };
    EventCategory GetCategory() const override { return EventCategory::System; }

    Canvas* canvas = nullptr;
    Theme* theme = nullptr;
};



// Returned by the callback to instruct iteration
struct VisitResult {
    // If false stops iteration and exit from all visit calls
    bool shouldContinue = true;
    // Skip iterating children of a current widget
    bool shouldSkipChildren = false;

    static auto Continue() { return VisitResult{true, false}; }
    static auto Exit() { return VisitResult{false, false}; }
    static auto SkipChildren() { return VisitResult{true, true}; }
};
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
    virtual ~Object() {
        if (refCounter_) {
            refCounter_->OnDestructed();
        }
    }

    template <class T>
    WeakPtr<T> GetWeakAs() {
        DASSERT_F(this->As<T>(), "Cannot cast {} to the class {}", GetDebugID(),
                  T::GetStaticClassName());
        if (!refCounter_) {
            refCounter_ = new util::RefCounter();
        }
        return WeakPtr<T>{this->As<T>(), refCounter_};
    }

    WeakPtr<Object> GetWeak() { return GetWeakAs<Object>(); }

    // Returns a debug identifier of this object
    // [MyWidgetClassName: <4 bytes of adress> "MyObjectID"]
    std::string GetDebugID() const;

protected:
    Object() = default;
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

private:
    // Used for custom WeakPtr to this
    util::RefCounter* refCounter_ = nullptr;
};

/*
 * Base class for ui components
 */
class Widget : public Object {
    WIDGET_CLASS(Widget, Object)
public:
    // Notify parents that we are about to be destructed
    virtual void Destroy() {
        std::unique_ptr<Widget> self = GetParent()->Orphan(this);
        // Destructed here
    }

    // Calls a visitor callback on this object and children if bRecursive ==
    // true
    virtual VisitResult VisitChildren(const WidgetVisitor& visitor,
                                      bool bRecursive = false) {
        return VisitResult::Continue();
    }
    VisitResult VisitChildrenRecursively(const WidgetVisitor& visitor) {
        return VisitChildren(visitor, true);
    }

    // Visits parents in the tree recursively
    void VisitParent(const WidgetVisitor& visitor, bool bRecursive = true) {
        for (auto parent = parent_; parent; parent = parent->GetParent()) {
            const auto result = visitor(parent);
            if (!result.shouldContinue || !bRecursive)
                return;
        }
    }

    // Attaches this widget to a parent slot
    virtual void Parent(std::unique_ptr<Widget>&& widget) {
        DASSERT_F(false, "Class {} cannot have children.", GetClassName());
    }

    [[nodiscard]] virtual std::unique_ptr<Widget> Orphan(Widget* widget) {
        DASSERT_F(false, "Class {} cannot have children.", GetClassName());
        return {};
    }

    virtual void OnParented(Widget* parent) { parent_ = parent; };
    virtual void OnOrphaned() { parent_ = nullptr; }

    virtual bool OnEvent(IEvent* event) {
        if (auto* log = event->As<DebugLogEvent>()) {
            log->archive->PushObject(GetClassName().data(), this, GetParent());
            DebugSerialize(*log->archive);
            return true;
        }
        return false;
    };

    // Update this widget configuration with the new one
    // Protected copy constructor should be provided
    // Called after rebuild during diffing the trees
    // Only widgets that can be referenced and containt some internal state
    // should override this
    // @return false - no need to update this widget, just replace
    virtual bool UpdateWith(const Widget* newWidget) { return false; }

    virtual void DebugSerialize(PropertyArchive& archive) {}

    // TODO: Move to LayoutWidget
    // Transforms a point in local space into a point in global space
    // recursively iterating parents
    virtual Point TransformLocalToGlobal(Point position) const {
        return parent_ ? parent_->TransformLocalToGlobal(position) : position;
    }

    bool DispatchToParent(IEvent* event) {
        if (parent_) {
            return parent_->OnEvent(event);
        }
        return false;
    }

    void DispatchNotification(Notification* notification) {
        for (auto* ancestor = parent_; ancestor;
             ancestor = ancestor->GetParent()) {
            auto handled = ancestor->OnEvent(notification);
            if (handled) {
                ++notification->depth;
            }
        }
    }

public:
    constexpr StringID GetID() const { return id_; }
    constexpr void SetID(StringID id) { id_ = id; }

    constexpr Widget* GetParent() { return parent_; }
    constexpr const Widget* GetParent() const { return parent_; }

    // Gets nearest parent in the tree of the class T
    template <WidgetSubclass T>
    const T* FindAncestorOfClass() const {
        for (auto* parent = GetParent(); parent; parent = parent->GetParent()) {
            if (parent->IsA<T>())
                return static_cast<const T*>(parent);
        }
        return nullptr;
    }

    template <WidgetSubclass T>
    T* FindAncestorOfClass() {
        return const_cast<T*>(std::as_const(*this).FindAncestorOfClass<T>());
    }

    // Looks for a child with specified class
    // @param bRecursive - Looks among children of children
    template <WidgetSubclass T>
    T* FindChildOfClass(bool bRecursive = true) {
        T* out = nullptr;
        VisitChildren(
            [&](Widget* child) {
                if (child->IsA<T>()) {
                    out = static_cast<T*>(child);
                    return VisitResult::Exit();
                }
                return VisitResult::Continue();
            },
            bRecursive);
        return out;
    }

protected:
    Widget(StringID id) : id_(id), parent_(nullptr) {}

    // Must call super
    // TODO: maybe if ids are different do not update
    void CopyConfiguration(const Widget& other) { id_ = other.id_; }

private:
    // Optional user defined ID
    StringID id_;
    // Our parent widget who manages our lifetime and passes events
    Widget* parent_;
};


/*
 * Abstract widget container that has one child
 */
class SingleChildWidget : public Widget {
    WIDGET_CLASS(SingleChildWidget, Widget)
public:
    void Parent(std::unique_ptr<Widget>&& widget) override {
        DASSERT(widget && !widget->GetParent() && !child_);
        child_ = std::move(widget);
        child_->OnParented(this);
    }

    NODISCARD std::unique_ptr<Widget> Orphan(Widget* widget) override {
        DASSERT(widget);
        if (child_.get() == widget) {
            child_->OnOrphaned();
            return std::move(child_);
        }
        return {};
    }
    NODISCARD std::unique_ptr<Widget> OrphanChild() {
        if (GetChild()) {
            return Orphan(GetChild());
        }
        return {};
    }

    bool DispatchToChildren(IEvent* event) {
        if (child_) {
            return child_->OnEvent(event);
        }
        return false;
    }

    VisitResult VisitChildren(const WidgetVisitor& visitor,
                              bool bRecursive = true) final {
        if (!child_)
            return VisitResult::Continue();

        auto result = visitor(child_.get());
        if (!result.shouldContinue)
            return VisitResult::Exit();

        if (bRecursive && !result.shouldSkipChildren) {
            result = child_->VisitChildren(visitor, bRecursive);
            if (!result.shouldContinue)
                return VisitResult::Exit();
        }
        return VisitResult::Continue();
    }

    bool OnEvent(IEvent* event) override {
        if (auto* log = event->As<DebugLogEvent>()) {
            log->archive->PushObject(GetDebugID(), (Widget*)this, GetParent());
            DebugSerialize(*log->archive);
            DispatchToChildren(event);
            return true;
        }
        return Widget::OnEvent(event);
    };

    Widget* GetChild() { return child_.get(); }

protected:
    SingleChildWidget(const std::string& id = {}) : Widget(id), child_() {}

private:
    std::unique_ptr<Widget> child_;
};


class StatefulWidget;

/*
 * Contains high level user defined state but subclassing it
 * A ui is a representation of this state
 * When the state becomes dirty a Build() is called to update the ui
 * representation of the data
 */
class WidgetState : public Object {
    WIDGET_CLASS(WidgetState, Object)
public:
    friend class StatefulWidget;

    WidgetState() = default;
    ~WidgetState();

    WidgetState(const WidgetState&) = delete;
    WidgetState& operator=(const WidgetState&) = delete;

    virtual std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&& child) = 0;

    void MarkNeedsRebuild();

    void SetVisibility(bool isVisible) {
        isVisible_ = isVisible;
        MarkNeedsRebuild();
    }

    // TODO: rework this, maybe just return nullptr on rebuild
    bool IsVisible() { return isVisible_; }

    // Called after build
    // Could be null if the widget tree is destructed before the state
    void SetWidget(StatefulWidget* widget) {
        widget_ = widget;
        // If our widget has been unmounted and destroyes mark us dirty
        // so we will be built at the next mount
        if (!widget_) {
            needsRebuild_ = true;
        }
    }

    StatefulWidget* GetWidget() { return widget_; }

private:
    bool isVisible_ = true;
    bool needsRebuild_ = true;
    // TODO: maybe use a WeakPtr here because if we forget to
    // rebind a new widget during rebuild, this will point to the deleted old
    // widget
    StatefulWidget* widget_ = nullptr;
};


/*
 * Owns widgets build by the bound state but the state
 * is owned by the user nad managed externally, and when the user changes the
 * state it triggers rebuild, and a result of the rebuild is parented to this
 * widget
 */
class StatefulWidget : public SingleChildWidget {
    WIDGET_CLASS(StatefulWidget, SingleChildWidget)
public:
    static std::unique_ptr<StatefulWidget> New(
        WidgetState* state,
        std::unique_ptr<Widget>&& child = {}) {
        return std::make_unique<StatefulWidget>(state, std::move(child));
    }

    StatefulWidget(WidgetState* state, std::unique_ptr<Widget>&& child = {})
        : state_(state), child_(std::move(child)) {}

    ~StatefulWidget() {
        if (state_) {
            state_->SetWidget(nullptr);
        }
    }

    void DebugSerialize(PropertyArchive& archive) override {
        SingleChildWidget::DebugSerialize(archive);
        archive.PushProperty("State", state_ ? state_->GetDebugID() : "null");
        archive.PushProperty("Dirty", state_ ? state_->needsRebuild_ : false);
    }

    void MarkNeedsRebuild() {
        state_->needsRebuild_ = true;
        Application::Get()->RequestRebuild(this);
    }

    std::unique_ptr<Widget> Build() {
        DASSERT_F(state_, "{} doesn't have a state.", GetDebugID());
        // If we are rebuilding this widget we need to provide old [child] into
        // state_.Build() because on first build it's provided from the owner
        // but on rebuild we need to provide it here
        const auto isRebuilding = state_->GetWidget() == this;
        if (isRebuilding && cachedChild_) {
            child_ = cachedChild_->GetParent()->Orphan(cachedChild_.Get());
        } else if (child_) {
            cachedChild_ = child_->GetWeak();
        }
        state_->needsRebuild_ = false;
        return state_->Build(std::move(child_));
    }

    bool NeedsRebuild() const {
        DASSERT_F(state_, "{} doesn't have a state.", GetDebugID());
        // If the child is provided by the owner, we need to force the state
        // rebuild
        if (child_) {
            state_->needsRebuild_ = true;
        }
        return state_->needsRebuild_;
    }

    // Bind this widget to the state and unbind the old one
    void RebindToState() {
        DASSERT_F(state_, "{} doesn't have a state.", GetDebugID());
        auto* old = state_->GetWidget();
        state_->SetWidget(this);

        if (old && old != this) {
            old->state_ = nullptr;
        }
    }

    WidgetState* GetState() { return state_; }

private:
    WidgetState* state_;
    std::unique_ptr<Widget> child_;
    WeakPtr<Widget> cachedChild_;
};

inline void WidgetState::MarkNeedsRebuild() {
    DASSERT_F(widget_, "State doesn't have a widget.");
    widget_->MarkNeedsRebuild();
}

inline WidgetState::~WidgetState() {
    if (widget_) {
        widget_->Destroy();
    }
}


/*
 * A widget which has size and position
 * And possibly can be drawn
 */
class LayoutWidget : public Widget {
    WIDGET_CLASS(LayoutWidget, Widget)
public:
    bool OnEvent(IEvent* event) override {
        if (auto* hitTest = event->As<HitTestEvent>()) {
            if (!IsVisible())
                return false;
            auto hitPosLocalSpace = hitTest->GetLastHitPos();

            if (GetRect().Contains(hitPosLocalSpace)) {
                hitTest->PushItem(this, hitPosLocalSpace - GetOrigin());
                return true;
            }
            return false;
        }
        return Widget::OnEvent(event);
    };

    void DebugSerialize(PropertyArchive& archive) override {
        Widget::DebugSerialize(archive);
        archive.PushProperty("Visible", !isHidden_);
        archive.PushProperty("FloatLayout", (bool)isFloatLayout_);
        archive.PushProperty("AxisMode", std::format("{}", axisMode_));
        archive.PushProperty("Origin", origin_);
        archive.PushProperty("Size", size_);
        archive.PushStringProperty("LayoutStyleSelector",
                                   layoutStyle_ ? *layoutStyle_->name : "null");
    }

    Point TransformLocalToGlobal(Point localPos) const override {
        return GetParent()
                   ? GetParent()->TransformLocalToGlobal(localPos + origin_)
                   : localPos + origin_;
    }

public:
    // Looks for a nearest LayoutWidget down the tree starting from widget
    static LayoutWidget* FindNearest(Widget* widget) {
        if (widget->IsA<LayoutWidget>())
            return widget->As<LayoutWidget>();
        return widget->FindChildOfClass<LayoutWidget>();
    }

    // Finds the topmost widget up the tree that is not another layout widget
    // Because LayoutWidget serves as a base for widgets composition
    // Other widgets that wrap this widget will use it for hovering, clicking,
    // moving behaviour
    Widget* FindTopmost() {
        Widget* topmost = this;

        for (auto* parent = GetParent(); parent && !parent->IsA<LayoutWidget>();
             parent = parent->GetParent()) {
            topmost = parent;
        }
        return topmost;
    }

    SizeMode GetAxisMode() const { return axisMode_; }
    void SetAxisMode(SizeMode mode) { axisMode_ = mode; }
    void SetAxisMode(Axis axis, AxisMode mode) { axisMode_[axis] = mode; }

    // Hiddent objects won't draw themselves and won't handle hovering
    // but layout update should be managed by the parent
    void SetVisibility(bool bVisible) { isHidden_ = !bVisible; }
    bool IsVisible() const { return !isHidden_; }

    // Should be called by subclasses
    void SetOrigin(Point pos) { origin_ = pos; }
    void SetOrigin(float inX, float inY) { origin_ = {inX, inY}; }
    void SetOrigin(Axis axis, float pos) { origin_[axis] = pos; }

    void Translate(float2 offset) { origin_ += offset; }
    void Translate(float inX, float inY) {
        origin_.x += inX;
        origin_.y += inY;
    }

    void SetSize(float2 size) { size_ = size; }
    void SetSize(Axis axis, float size) { size_[(int)axis] = size; }
    void SetSize(float inX, float inY) { size_ = {inX, inY}; }

    Rect GetRect() const { return {origin_, size_}; }
    // Returns Rect of this widget in global coordinates
    Rect GetRectGlobal() const {
        return Rect(GetParent() ? GetParent()->TransformLocalToGlobal(origin_)
                                : origin_,
                    size_);
    }
    float2 GetSize() const { return size_; }
    float GetSize(Axis axis) const { return size_[axis]; }

    Point GetOrigin() const { return origin_; }
    Point GetTransform() const { return origin_; }

    // Size + Margins
    float2 GetOuterSize() {
        const auto size = GetSize();
        const auto margins =
            GetLayoutStyle() ? GetLayoutStyle()->margins : Margins{};
        return {size.x + margins.Left + margins.Right,
                size.y + margins.Top + margins.Bottom};
    }

    // Size - Paddings
    float2 GetInnerSize() {
        const auto size = GetSize();
        const auto paddings =
            GetLayoutStyle() ? GetLayoutStyle()->paddings : Paddings{};
        return {size.x - paddings.Left - paddings.Right,
                size.y - paddings.Top - paddings.Bottom};
    }

    void SetLayoutStyle(const LayoutStyle* style) { layoutStyle_ = style; }
    const LayoutStyle* GetLayoutStyle() const { return layoutStyle_; }

    // Widget's position won't be affected by parent's layout events
    void SetFloatLayout(bool bEnable = true) { isFloatLayout_ = bEnable; }
    bool IsFloatLayout() const { return isFloatLayout_; }

public:
    virtual bool OnHitTest(HitTestEvent& event, float2 position) {
        if (GetRect().Contains(position)) {
            const auto internalPosition = position - GetOrigin();
            event.PushItem(this, internalPosition);
            HitTestChildren(event, internalPosition);
            return true;
        }
        return false;
    }

    virtual bool HitTestChildren(HitTestEvent& event, float2 position) {
        return false;
    }

    // Returns size after layout is performed
    virtual float2 OnLayout(const LayoutConstraints& event) {
        const auto margins = GetLayoutStyle()->margins;

        if (!isFloatLayout_) {
            SetOrigin(event.rect.TL() + margins.TL());
        }
        for (auto axis : axes2D) {
            if (GetAxisMode()[axis] == AxisMode::Expand) {
                DASSERT_F(!event.parent || event.parent->GetAxisMode()[axis] !=
                                               AxisMode::Shrink,
                          "{} axis {} mode is set to AxisMode::Expand while "
                          "parent's {} axis mode is set to AxisMode::Shrink.",
                          GetDebugID(), axis == Axis::X ? "X" : "Y",
                          event.parent->GetDebugID());
                SetSize(axis, event.rect.Size()[axis] - margins.Size()[axis]);
            }
        }
        return GetOuterSize();
    }

    // Should be called by subclasses at the end of their OnLayout()
    void OnPostLayout() {
        if (notifyOnUpdate_) {
            LayoutNotification e;
            e.rectLocal = GetRect();
            e.source = this;
            DispatchNotification(&e);
        }
    }

protected:
    LayoutWidget(const LayoutStyle* style = nullptr,
                 SizeMode axisMode = SizeMode::Fixed(),
                 bool notifyOnUpdate = false,
                 StringID id = {})
        : Widget(id)
        , isHidden_(false)
        , isFloatLayout_(false)
        , notifyOnUpdate_(notifyOnUpdate)
        , axisMode_(axisMode)
        , layoutStyle_(style) {}

    void CopyConfiguration(const LayoutWidget& other) {
        Widget::CopyConfiguration(other);
        isFloatLayout_ = other.isFloatLayout_;
        notifyOnUpdate_ = other.notifyOnUpdate_;
        axisMode_ = other.axisMode_;
        layoutStyle_ = other.layoutStyle_;
    }

private:
    uint8_t isHidden_ : 1;
    uint8_t isFloatLayout_ : 1;
    // Send notifications to ancestors when updated
    uint8_t notifyOnUpdate_ : 1;
    SizeMode axisMode_;
    // Position in pixels relative to parent origin
    Point origin_;
    float2 size_;
    // Style object in the Theme
    const LayoutStyle* layoutStyle_;
};


/*
 * LayoutWidget container that has one child
 */
class SingleChildLayoutWidget : public LayoutWidget {
    WIDGET_CLASS(SingleChildLayoutWidget, LayoutWidget)
public:
    void Parent(std::unique_ptr<Widget>&& widget) override {
        DASSERT(widget && !widget->GetParent() && !child_);
        child_ = std::move(widget);
        child_->OnParented(this);
    }

    NODISCARD std::unique_ptr<Widget> Orphan(Widget* widget) override {
        DASSERT(widget);
        if (child_.get() == widget) {
            child_->OnOrphaned();
            return std::move(child_);
        }
        return {};
    }

    bool DispatchToChildren(IEvent* event) {
        if (child_) {
            return child_->OnEvent(event);
        }
        return false;
    }

    VisitResult VisitChildren(const WidgetVisitor& visitor,
                              bool bRecursive = true) final {
        if (!child_)
            return VisitResult::Continue();

        const auto result = visitor(child_.get());
        if (!result.shouldContinue)
            return VisitResult::Exit();

        if (bRecursive && !result.shouldSkipChildren) {
            const auto result = child_->VisitChildren(visitor, bRecursive);
            if (!result.shouldContinue)
                return VisitResult::Exit();
        }
        return VisitResult::Continue();
    }

    bool OnEvent(IEvent* event) override {
        if (auto* log = event->As<DebugLogEvent>()) {
            log->archive->PushObject(GetDebugID(), (Widget*)this, GetParent());
            DebugSerialize(*log->archive);
            DispatchToChildren(event);
            return true;
        }
        if (event->IsA<HitTestEvent>()) {
            DASSERT_M(false, "Deprecated");
            return false;
        }
        return LayoutWidget::OnEvent(event);
    }

public:
    bool HitTestChildren(HitTestEvent& event, float2 position) override {
        if (!child_) {
            return false;
        }
        if (auto* layoutChild = FindChildOfClass<LayoutWidget>()) {
            return layoutChild->OnHitTest(event, position);
        }
        return false;
    }

    float2 OnLayout(const LayoutConstraints& rect) override {
        const auto layoutInfo = *GetLayoutStyle();
        const auto margins = layoutInfo.margins;

        if (!IsFloatLayout()) {
            SetOrigin(rect.rect.TL() + margins.TL());
        }
        for (auto axis : axes2D) {
            if (GetAxisMode()[axis] == AxisMode::Expand) {
                SetSize(axis, rect.rect.Size()[axis] - margins.Size()[axis]);
            }
        }
        if (!child_) {
            return GetSize() + margins.Size();
        }
        LayoutConstraints e;
        e.parent = this;
        e.rect = Rect(layoutInfo.paddings.TL(),
                      GetSize() - layoutInfo.paddings.Size());

        if (auto* layoutWidget = FindChildOfClass<LayoutWidget>()) {
            const auto childOuterSize = layoutWidget->OnLayout(e);
            layoutWidget->OnPostLayout();

            for (auto axis : axes2D) {
                if (GetAxisMode()[axis] == AxisMode::Shrink) {
                    SetSize(axis, childOuterSize[axis] +
                                      layoutInfo.paddings.Size()[axis]);
                }
            }
        }
        return GetSize() + margins.Size();
    }

protected:
    SingleChildLayoutWidget(StringID styleName,
                            SizeMode axisMode = SizeMode::Shrink(),
                            bool notifyOnUpdate = false,
                            StringID id = {})
        : LayoutWidget(Application::Get()
                           ->GetTheme()
                           ->Find(styleName)
                           ->Find<LayoutStyle>(),
                       axisMode,
                       notifyOnUpdate,
                       id)
        , child_() {}

public:
    std::unique_ptr<Widget> child_;
};



/*
 * LayoutWidget container that has multiple children
 */
class MultiChildLayoutWidget : public LayoutWidget {
    WIDGET_CLASS(MultiChildLayoutWidget, LayoutWidget)
public:
    void Parent(std::unique_ptr<Widget>&& widget) override {
        DASSERT(widget && !widget->GetParent());
        auto* w = widget.get();
        children_.push_back(std::move(widget));
        w->OnParented(this);
    }

    std::unique_ptr<Widget> Orphan(Widget* widget) override {
        DASSERT(widget);
        for (auto it = children_.begin(); it != children_.end(); ++it) {
            if (it->get() == widget) {
                auto out = std::move(*it);
                children_.erase(it);
                widget->OnOrphaned();
                return out;
            }
        }
        return {};
    }

    bool DispatchToChildren(IEvent* event) {
        for (auto& child : children_) {
            auto result = child->OnEvent(event);

            if (result && !event->IsBroadcast()) {
                return result;
            }
        }
        return false;
    }

    void GetVisibleChildren(std::vector<LayoutWidget*>* outVisibleChildren) {
        outVisibleChildren->reserve(children_.size());

        for (auto& child : children_) {
            auto layoutChild = child->As<LayoutWidget>();

            if (!layoutChild) {
                layoutChild = child->FindChildOfClass<LayoutWidget>();
            }

            if (layoutChild && layoutChild->IsVisible()) {
                outVisibleChildren->push_back(layoutChild);
            }
        }
    }

    VisitResult VisitChildren(const WidgetVisitor& visitor,
                              bool bRecursive = false) final {
        for (auto& child : children_) {
            const auto result = visitor(child.get());
            if (!result.shouldContinue)
                return VisitResult::Exit();

            if (bRecursive && !result.shouldSkipChildren) {
                const auto result = child->VisitChildren(visitor, bRecursive);
                if (!result.shouldContinue)
                    return VisitResult::Exit();
            }
        }
        return VisitResult::Continue();
    }

    bool OnEvent(IEvent* event) override {
        if (event->IsA<HitTestEvent>()) {
            if (LayoutWidget::OnEvent(event)) {
                for (auto& child : children_) {
                    auto* layoutChild = LayoutWidget::FindNearest(child.get());
                    if (layoutChild && layoutChild->OnEvent(event)) {
                        break;
                    }
                }
                return true;
            }
            return false;
        }
        if (auto* log = event->As<DebugLogEvent>()) {
            log->archive->PushObject(GetDebugID(), (Widget*)this, GetParent());
            DebugSerialize(*log->archive);
            DispatchToChildren(event);
            return true;
        }
        return LayoutWidget::OnEvent(event);
    }

    bool HitTestChildren(HitTestEvent& event, float2 position) override {
        if (children_.empty()) {
            return false;
        }
        for (auto& child : std::views::reverse(children_)) {
            if (auto* layoutChild = LayoutWidget::FindNearest(child.get())) {
                if (layoutChild->OnHitTest(event, position)) {
                    return true;
                }
            }
        }
        return false;
    }

public:
    bool Empty() const { return children_.empty(); }

protected:
    MultiChildLayoutWidget(const LayoutStyle* style = nullptr,
                           SizeMode axisMode = SizeMode::Shrink(),
                           bool notifyOnUpdate = false,
                           const std::string& id = {})
        : LayoutWidget(style, axisMode, notifyOnUpdate, id), children_() {}

    auto begin() { return children_.begin(); }
    auto end() { return children_.end(); }

    auto begin() const { return children_.begin(); }
    auto end() const { return children_.end(); }

private:
    std::vector<std::unique_ptr<Widget>> children_;
};



class MouseRegionBuilder;

using MouseEnterEventCallback = std::function<void()>;
using MouseLeaveEventCallback = std::function<void()>;
using MouseHoverEventCallback = std::function<void(const HoverEvent&)>;
using MouseButtonEventCallback = std::function<void(const MouseButtonEvent&)>;
using MouseDragEventCallback = std::function<void(const MouseDragEvent&)>;

// Must return true if wants to consume event
using MouseScrollEventCallback = std::function<bool(const MouseScrollEvent&)>;

struct MouseRegionConfig {
    MouseEnterEventCallback onMouseEnter;
    MouseLeaveEventCallback onMouseLeave;
    MouseHoverEventCallback onMouseHover;
    MouseButtonEventCallback onMouseButton;
    MouseDragEventCallback onMouseDrag;
    MouseScrollEventCallback onMouseScroll;
    // Whether to handle events even if other MouseRegion widget
    //     has already handled the event
    bool bHandleHoverAlways = false;
    bool bHandleButtonAlways = false;
};

/*
 * Widget that detects mouse events
 * Allows to receive events even if handled by children
 * Uses underlying LayoutWidget for hit detection
 * Other widget types can still receive these events but only if not captured by
 * children
 */
class MouseRegion : public SingleChildWidget {
    WIDGET_CLASS(MouseRegion, SingleChildWidget)
public:
    static MouseRegionBuilder Build();

    bool OnEvent(IEvent* event) override {
        if (auto* hoverEvent = event->As<HoverEvent>()) {
            if (hoverEvent->bHoverEnter) {
                if (config_.onMouseEnter) {
                    config_.onMouseEnter();
                } else if (config_.onMouseHover) {
                    config_.onMouseHover(*hoverEvent);
                }
            } else if (hoverEvent->bHoverLeave) {
                if (config_.onMouseLeave) {
                    config_.onMouseLeave();
                }
            } else if (config_.onMouseHover) {
                config_.onMouseHover(*hoverEvent);
            }
            return config_.onMouseEnter || config_.onMouseHover;
        }
        if (auto* dragEvent = event->As<MouseDragEvent>()) {
            if (config_.onMouseDrag) {
                config_.onMouseDrag(*dragEvent);
                return true;
            }
            return config_.onMouseDrag || config_.onMouseButton;
        }
        if (auto* buttonEvent = event->As<MouseButtonEvent>()) {
            if (config_.onMouseButton) {
                config_.onMouseButton(*buttonEvent);
                return true;
            }
            return config_.onMouseDrag || config_.onMouseButton;
        }
        if (auto* scrollEvent = event->As<MouseScrollEvent>()) {
            if (config_.onMouseScroll) {
                return config_.onMouseScroll(*scrollEvent);
            }
            return false;
        }
        return SingleChildWidget::OnEvent(event);
    }

    bool UpdateWith(const Widget* newWidget) override {
        DASSERT_F(GetClass() == MouseRegion::GetStaticClass(),
                  "UpdateWith() must be overriden in subclasses, "
                  "otherwise during widget rebuilding and diffing only "
                  "superclass part will be updated, which leads to ub. "
                  "This widget class is '{}'.",
                  GetClassName());
        if (auto* w = newWidget->As<MouseRegion>()) {
            CopyConfiguration(*w);
            return true;
        }
        return false;
    }

    void DebugSerialize(PropertyArchive& archive) override {
        SingleChildWidget::DebugSerialize(archive);
        archive.PushProperty("AlwaysReceiveHover", config_.bHandleHoverAlways);
        archive.PushProperty("AlwaysReceiveButton",
                             config_.bHandleButtonAlways);

        std::string str;
        if (config_.onMouseEnter) {
            str.append("onMouseEnter");
            str.append(" | ");
        }
        if (config_.onMouseHover) {
            str.append("onMouseHover");
            str.append(" | ");
        }
        if (config_.onMouseLeave) {
            str.append("onMouseLeave");
            str.append(" | ");
        }
        if (config_.onMouseButton) {
            str.append("onMouseButton");
            str.append(" | ");
        }
        if (config_.onMouseDrag) {
            str.append("onMouseDrag");
            str.append(" | ");
        }
        if (config_.onMouseScroll) {
            str.append("onMouseScroll");
            str.append(" | ");
        }
        // Delete " | " at the end
        if (!str.empty() && str.back() == ' ') {
            str = str.substr(0, str.size() - 3);
        }
        archive.PushProperty("Callbacks", str);
    }

    bool ShouldAlwaysReceiveHover() const { return config_.bHandleHoverAlways; }
    bool ShouldAlwaysReceiveButton() const {
        return config_.bHandleButtonAlways;
    }

protected:
    MouseRegion(const MouseRegionConfig& config) : config_(config) {}

    void CopyConfiguration(const MouseRegion& other) {
        SingleChildWidget::CopyConfiguration(other);
        config_ = other.config_;
    }

    friend class MouseRegionBuilder;

private:
    MouseRegionConfig config_;
};

class MouseRegionBuilder {
public:
    auto& ID(StringID id) {
        id_ = id;
        return *this;
    }
    auto& OnMouseButton(const MouseButtonEventCallback& c) {
        config_.onMouseButton = c;
        return *this;
    }
    auto& OnMouseDrag(const MouseDragEventCallback& c) {
        config_.onMouseDrag = c;
        return *this;
    }
    auto& OnMouseEnter(const MouseEnterEventCallback& c) {
        config_.onMouseEnter = c;
        return *this;
    }
    auto& OnMouseLeave(const MouseLeaveEventCallback& c) {
        config_.onMouseLeave = c;
        return *this;
    }
    auto& OnMouseHover(const MouseHoverEventCallback& c) {
        config_.onMouseHover = c;
        return *this;
    }
    auto& OnMouseScroll(const MouseScrollEventCallback& c) {
        config_.onMouseScroll = c;
        return *this;
    }
    auto& HandleHoverAlways(bool b = true) {
        config_.bHandleHoverAlways = b;
        return *this;
    }
    auto& HandleButtonAlways(bool b = true) {
        config_.bHandleButtonAlways = b;
        return *this;
    }
    auto& Child(std::unique_ptr<Widget>&& child) {
        child_ = std::move(child);
        return *this;
    }

    std::unique_ptr<MouseRegion> New() {
        DASSERT_F(
            [&]() {
                if (config_.onMouseHover || config_.onMouseEnter) {
                    return (bool)config_.onMouseLeave;
                }
                return true;
            }(),
            "OnMouseLeave callback should be set if OnMouseEnter or "
            "OnMouseHover is set");

        std::unique_ptr<MouseRegion> out(new MouseRegion(config_));
        out->SetID(id_);
        out->Parent(std::move(child_));
        return out;
    }

private:
    StringID id_;
    MouseRegionConfig config_;
    std::unique_ptr<Widget> child_;
};

inline MouseRegionBuilder MouseRegion::Build() {
    return {};
}



using EventCallback = std::function<bool(IEvent*)>;

/*
 * TODO: maybe rename into NotificationListener and separate notificatoins and
 * events Calls the user provided callback when an event is received
 */
class EventListener : public SingleChildWidget {
    WIDGET_CLASS(EventListener, SingleChildWidget)
public:
    static auto New(const EventCallback& callback,
                    std::unique_ptr<Widget>&& child) {
        auto out = std::make_unique<EventListener>();
        out->callback_ = callback;
        out->Parent(std::move(child));
        return out;
    }

    bool OnEvent(IEvent* event) override {
        if (callback_ && callback_(event)) {
            return true;
        }
        return SingleChildWidget::OnEvent(event);
    }

private:
    EventCallback callback_;
};



// Iterate ancestors of a widget via 
// "for(Widget* ancestor: IterateAncestors(widget))"
auto IterateAncestors(Widget* widget) {
    class AncestorIterator final {
    public:
        explicit AncestorIterator(Widget* widget)
            : current(widget) {}

        AncestorIterator(const AncestorIterator& other): current(other.current) {}

        AncestorIterator& operator=(const AncestorIterator& other) {
            current = other.current;
        }

        class Iterator final {
        public:
            explicit Iterator(Widget* widget): current(widget) {}

            Iterator& operator++() {
                DASSERT(current);
                current = current->GetParent();
                return *this;
            }

            bool operator==(const Iterator& other) const = default;
            bool operator!=(const Iterator& other) const = default;

            Widget* operator*() const {
                return current;
            }

        private:
            Widget* current;
        };

        Iterator begin() const { return Iterator{current}; }
        Iterator end() const { return Iterator{nullptr}; }

    private:
        Widget* current;
    };
    return AncestorIterator(widget);
}


}  // namespace ui

constexpr void ui::HitStack::Push(LayoutWidget* widget, Point posLocal) {
    stack.push_back(HitData{widget->GetWeak(), posLocal});
}

template <>
struct std::formatter<ui::SizeMode, char> {
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <class FmtContext>
    FmtContext::iterator format(const ui::SizeMode& sizeMode,
                                FmtContext& ctx) const {
        const auto out =
            std::format("{}:{}", to_string(sizeMode.x), to_string(sizeMode.y));
        return std::ranges::copy(std::move(out), ctx.out()).out;
    }
};

inline std::string ui::Object::GetDebugID() const {
    if (auto* w = this->As<Widget>(); w && !w->GetID().Empty()) {
        return std::format("[{}: {:x} \"{}\"]", GetClassName(),
                           (uintptr_t)this & 0xffff, w->GetID());
    } else {
        return std::format("[{}: {:x}]", GetClassName(),
                           (uintptr_t)this & 0xffff);
    }
}