#include "framework.h"
#include "widgets.h"
#include "containers.h"
#include "focus.h"

#include "runtime/native_window.h"
#include "runtime/ui_renderer.h"
#include "runtime/time_utils.h"

#include "thirdparty/optik/include/optick.config.h"
#include "thirdparty/optik/include/optick.h"

#include <stack>


namespace ui {

using Timer = Timer<std::chrono::milliseconds>;
using RendererDrawlist = ::DrawList;

class WindowController;
class Application;
class ApplicationImpl;

constexpr u32	 g_TooltipDelayMs = 500;
constexpr u8	 g_DefaultFontSize = 13;

INativeWindow* g_OSWindow = nullptr;
Renderer* g_Renderer = nullptr;
ApplicationImpl* g_Application = nullptr;


namespace names {
	constexpr auto RootBackgroundWindowID = "RootBackgroundWindow";
	constexpr auto debugOverlayWindowID = "DebugOverlayWindow";
}




/*
* Array of timers registered by widgets
* Could be used for animations and delayed actions
*	like a popup opening or tooltip opening
*/
class TimerList {
public:

	using TimerCallback = std::function<bool()>;
	using TimePoint = std::chrono::high_resolution_clock::time_point;
	using TimerHandle = void*;

	struct Timer {
		WeakPtr<Object>	object;
		TimerCallback	callback;
		u64				periodMs;
		TimePoint		timePoint;
	};

public:

	TimerHandle AddTimer(Object* object, const TimerCallback& callback, u64 periodMs) {
		timers_.emplace_back(Timer(object->GetWeak(), callback, periodMs, Now()));
		return (TimerHandle)&timers_.back();
	}

	void		RemoveTimer(TimerHandle handle) {
		for(auto it = timers_.begin(); it != timers_.end(); ++it) {
			if(&*it == handle) {
				timers_.erase(it);
				break;
			}
		}
	}

	// Ticks timers and calls callbacks
	void		Tick() {
		if(timers_.empty()) return;

		const auto now = Now();
		std::vector<std::list<Timer>::const_iterator> pendingDelete;

		for(auto it = timers_.begin(); it != timers_.end(); ++it) {
			auto& timer = *it;

			if(!timer.object) {
				pendingDelete.push_back(it);
				continue;
			}

			if(DurationMs(timer.timePoint, now) >= timer.periodMs) {
				const auto bContinue = timer.callback();

				if(bContinue) {
					timer.timePoint = now;
				} else {
					pendingDelete.push_back(it);
				}
			}
		}

		for(auto& it : pendingDelete) {
			timers_.erase(it);
		}
	}

	size_t Size() const { return timers_.size(); }

private:

	constexpr u64 DurationMs(const TimePoint& p0, const TimePoint& p1) {
		return p1 >= p0
			? std::chrono::duration_cast<std::chrono::milliseconds>(p1 - p0).count()
			: std::chrono::duration_cast<std::chrono::milliseconds>(p0 - p1).count();
	}

	TimePoint Now() { return std::chrono::high_resolution_clock::now(); }

private:
	std::list<Timer> timers_;
};



/*
* Helper to draw text vertically in a sinle column
* for debuging
*/
class VerticalTextDrawer {

	constexpr static inline auto cursorOffset = float2(0.f, 15.f);
	constexpr static inline auto indent = 15.f;
public:

	VerticalTextDrawer(DrawList* drawList, Point cursor, Color color)
		: drawList_(drawList)
		, cursor_(cursor)
		, color_(color)
		, indent_(0) {}

	void DrawText(const std::string& text) {
		drawList_->DrawText(cursor_ + float2(indent_, 0.f), color_, text);
		cursor_ += cursorOffset;
	}

	void DrawText(std::string_view text) {
		drawList_->DrawText(cursor_ + float2(indent_, 0.f), color_, text.data());
		cursor_ += cursorOffset;
	}

	void DrawText(const char* text) {
		drawList_->DrawText(cursor_ + float2(indent_, 0.f), color_, text);
		cursor_ += cursorOffset;
	}

	template<typename ...ArgTypes>
	void DrawTextF(const std::format_string<ArgTypes...> format, ArgTypes... args) {
		drawList_->DrawText(cursor_ + float2(indent_, 0.f), color_, std::format(format, std::forward<ArgTypes>(args)...));
		cursor_ += cursorOffset;
	}

	void PushIndent() { indent_ += indent; }
	void PopIndent() { indent_ -= indent; }

private:
	Point		cursor_;
	DrawList*   drawList_;
	Color		color_;
	float		indent_;
};



class CanvasImpl final: public Canvas {
public:
	CanvasImpl(DrawList* drawList)
		: drawList(drawList) {}

	void DrawBox(Rect rect, const BoxStyle* style) override {
		if(style->opacity == 0.f) return;
		rect = Transform(rect);
		Rect backgroundRect;
		Rect borderRect;

		if(style->borderAsOutline) {
			backgroundRect = rect;
			borderRect = Rect(backgroundRect).Expand(style->borders);
		} else {
			borderRect = rect;
			backgroundRect = Rect(rect).Expand(
				(float)-style->borders.Top,
				(float)-style->borders.Right,
				(float)-style->borders.Bottom,
				(float)-style->borders.Left);
		}

		auto borderColor = ColorFloat4(style->borderColor);
		auto backgroundColor = ColorFloat4(style->backgroundColor);

		borderColor.a *= style->opacity;
		backgroundColor.a *= style->opacity;

		if(style->borders.Left || style->borders.Right || style->borders.Top || style->borders.Bottom) {

			if(backgroundColor.a != 1.f) {
				drawList->DrawRect(borderRect, ColorU32(borderColor), style->rounding, (DrawList::Corner)style->roundingMask);
			} else {
				drawList->DrawRectFilled(borderRect, ColorU32(borderColor), style->rounding, (DrawList::Corner)style->roundingMask);
			}
		}

		drawList->DrawRectFilled(backgroundRect, ColorU32(backgroundColor), style->rounding, (DrawList::Corner)style->roundingMask);
	}

	void DrawRect(Rect rect, Color color, bool bFilled = true) override {
		rect = Transform(rect);
		if(bFilled) {
			drawList->DrawRectFilled(rect, ColorU32(color));
		} else {
			drawList->DrawRect(rect, ColorU32(color));
		}
	}

	void DrawText(Point origin, const TextStyle* style, std::string_view textView) override {
		origin = Transform(origin);
		drawList->DrawText(origin, style->color, textView, style->fontSize, style->fontWeightBold, style->fontStyleItalic);
	}

	void ClipRect(Rect clipRect) override {
		Assert(!clipRect.Empty());
		clipRect = Transform(clipRect);
		context.clipRect = clipRect;
		context.hasClipRect = true;
	}

	void DrawClipRect(bool bFilled, Color color) {
		if(context.hasClipRect) {
			if(bFilled) {
				drawList->DrawRectFilled(context.clipRect, color);
			} else {
				drawList->DrawRect(context.clipRect, color);
			}
		}
	}

	void PushTransform(float2 transform) {
		context.transform = transform;
	}

	void SaveContext() {
		if(context.hasClipRect) {
			drawList->PushClipRect(context.clipRect);
		}
		cummulativeTransform += context.transform;
		contextStack.push_back(context);
		context.transform = {};
		context.clipRect = {};
		context.hasClipRect = false;
	}

	void RestoreContext() {
		Assert(!contextStack.empty());
		auto& frame = contextStack.back();
		cummulativeTransform -= frame.transform;

		if(frame.hasClipRect) {
			drawList->PopClipRect();
		}
		contextStack.pop_back();
	}

	void ClearContext() {
		while(!contextStack.empty()) {
			RestoreContext();
		}
		Assert(float2(std::round(cummulativeTransform.x), std::round(cummulativeTransform.y)) == float2());
	}

	bool HasClipRect() const {
		return context.hasClipRect;
	}

	Point Transform(Point point) {
		return point + cummulativeTransform;
	}

	Rect Transform(Rect rect) {
		return Rect(rect).Translate(cummulativeTransform);
	}

private:
	RendererDrawlist* 	 drawList = nullptr;

	struct Context {
		Point transform;
		Rect  clipRect;
		bool  hasClipRect = false;
	};
	Context              context;
	std::vector<Context> contextStack;
	float2               cummulativeTransform;
};

/*
* Helper to easily iterate over ancestors of a widget
* "for(auto* ancestor: AncestorIterator(widget))"
* Iterates ancestor of the widget
* An ancestor could be filtered by type
*/
template<class WidgetTypeFilter = Widget>
	requires std::derived_from<WidgetTypeFilter, Widget>
struct AncestorIterator {

	constexpr AncestorIterator(Widget* w, bool includeBegin = false)
		: begin_(w) 
		, includeBegin_(includeBegin)
	{}

	struct Iterator {

		constexpr Iterator& operator++() { 
			if constexpr(std::same_as<WidgetTypeFilter, Widget>) {
				if(w_) {
					w_ = w_->GetParent();
				}
			} else {
				if(w_) {
					w_ = w_->FindAncestorOfClass<WidgetTypeFilter>();
				}
			}
			return *this;
		}	

		constexpr bool operator==(const Iterator& rhs) const {
			return w_ == rhs.w_;
		}

		constexpr WidgetTypeFilter* operator*() {
			return w_->As<WidgetTypeFilter>();
		}

		Widget* w_ = nullptr;
	};

	constexpr Iterator begin() { 
		if(!begin_) {
			return {};
		}
		if(includeBegin_) {
			if constexpr(std::same_as<WidgetTypeFilter, Widget>) {
				return {begin_};
			}
			if(!begin_->IsA<WidgetTypeFilter>()) {
				return {begin_->FindAncestorOfClass<WidgetTypeFilter>()};
			}
			return {begin_};
		}	
		if constexpr(std::same_as<WidgetTypeFilter, Widget>) {
			return {begin_->GetParent()};
		}
		return {begin_->FindAncestorOfClass<WidgetTypeFilter>()};
	}

	constexpr Iterator end() { return {}; }

private:
	Widget* begin_        = nullptr;
	bool    includeBegin_ = true;
};



/*
* Helper adapter to store widget state between frames
* Stores weaks to widgets, so that widgets could be safely deleted
* Expired weaks will be skipped during iteration and other operations
*/
class WidgetList {
public:
	using Container = std::vector<WeakPtr<Widget>>;
	
	struct Iterator {
		using iterator = Container::iterator;

		constexpr Iterator& operator++() { 
			++it;
			while(it != se && it->Expired()) {
				++it;
			}
			return *this;
		}	

		constexpr operator bool() const { return it != se; }

		constexpr bool operator==(const Iterator& rhs) const {
			return it == rhs.it;
		}

		Widget* operator*() {
			return it->Get();
		}

		iterator it;
		iterator se;
	};

	WidgetList() {
		cont.reserve(10);
	}

	void Push(Widget* item) {
		cont.push_back(item->GetWeak());
	}

	void ForEach(const std::function<void(Widget*)>& fn) {
		for(auto& item: cont) {
			if(!item) continue;
			fn(item.Get());
		}
	}

	void Remove(Widget* item) {
		auto [it, se] = Find(item);
		if(it != cont.end()) {
			cont.erase(it);
		}
	}
	
	void Remove(Iterator it) {
		if(it.it != cont.end()) {
			cont.erase(it.it);
		}
	}
	
	Iterator Find(Widget* item) {
		for(auto it = cont.begin(); it != cont.end(); ++it) {
			if(!it->Expired() && *it == item) {
				return {it, cont.end()};
			}
		}
		return {cont.end(), cont.end()};
	}

	constexpr bool Contains(const Widget* item) const {
		for(auto& elem: cont) {
			if(!elem.Expired() && elem == item) {
				return true;
			}
		}
		return false;
	}

	constexpr void Clear() { cont.clear(); }
	constexpr bool Empty() const { return cont.empty(); }	

	Widget* Top() {
		return cont.empty() 
			? nullptr 
			: cont.front().GetChecked(); 
	}

	const Widget* Top() const {
		return cont.empty() 
			? nullptr 
			: cont.front().GetChecked(); 
	}

	std::string Print() const {
		std::string out;
		util::StringBuilder sb(&out);
		sb.PushIndent();
		sb.Line();

		for(auto& w: cont) {
			if(!w) continue;
			sb.Line(w->GetDebugID());
		}
		return out;
	}

	constexpr Iterator begin() {
		auto begin = cont.begin();
		// Find first not expired weak
		while(begin != cont.end() && begin->Expired()) {
			++begin;
		}
		return {begin, cont.end()};
	}

	constexpr Iterator end() {
		return {cont.end(), cont.end()};
	}

private:
	Container cont;
};

// Simple n^2 comparison
// Items that present only in 'left' are considered 'old'
// Items that present only in 'right' are considered 'new' 
// Items that present in both lists are ignored
void DiffListsSimple(WidgetList& left, 
			   		 WidgetList& right, 
			   		 const std::function<void(Widget*)>& onOld,
			   		 const std::function<void(Widget*)>& onNew) {
	auto rightCopy = right;
	for(auto* item: left) {
		auto found = rightCopy.Find(item);
		if(!found) {
			onOld(item);
		} else {
			rightCopy.Remove(found);
		}
	}
	for(auto* item: rightCopy) {
		onNew(item);
	}
}



/*
* Overlay that is drawn over root window
*/
class DebugOverlayWindow: public WidgetState {
public:

	std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) {
		if(!isVisible_) {
			return {};
		}
		return Container::Build()
			.ID(names::debugOverlayWindowID)
			.StyleClass(names::debugOverlayWindowID)
			.ClipContent(true)
			.PositionFloat({5, 5})
			.Child(TextBox::New(text_))
			.New();
	}

	void SetText(const std::string& text) {
		text_ = text;
		MarkNeedsRebuild();
	}

	bool isVisible_ = true;
	std::string text_;
};


/*
* Top object that handles input events and drawing
*/
class ApplicationImpl final:
	public ui::Application,
	public Widget {
public:

	ApplicationImpl(): Widget("Application") {}

	void Init() {
		OPTICK_EVENT("ui Init");
		g_Renderer = CreateRendererDX12();
		g_Renderer->Init(g_OSWindow);

		theme_.reset(new Theme());
		theme_->CreateDefaults(kDefaultFontSize);

		auto& debugOverlayStyle = theme_->Add(names::debugOverlayWindowID);
		debugOverlayStyle.Add<LayoutStyle>().Margins(5, 5).Paddings(5, 5);
		debugOverlayStyle.Add<BoxStyle>().FillColor("#454545dd").Rounding(4);

		RebuildFonts();
		debugOverlay_ = std::make_unique<DebugOverlayWindow>();
		Parent(StatefulWidget::New(debugOverlay_.get()), Layer::Overlay);

		g_OSWindow->SetOnCursorMoveCallback([](float x, float y) { g_Application->DispatchMouseMoveEvent({x, y}); });
		g_OSWindow->SetOnMouseButtonCallback([](KeyCode button, bool bPressed) { g_Application->DispatchMouseButtonEvent(button, bPressed); });
		g_OSWindow->SetOnMouseScrollCallback([](float scroll) { g_Application->DispatchMouseScrollEvent(scroll); });
		g_OSWindow->SetOnWindowResizedCallback([](float2 windowSize) { g_Application->DispatchOSWindowResizeEvent(windowSize); });
		g_OSWindow->SetOnKeyboardButtonCallback([](KeyCode button, bool bPressed) { g_Application->DispatchKeyEvent(button, bPressed); });
		g_OSWindow->SetOnCharInputCallback([](wchar_t character) { g_Application->DispatchCharInputEvent(character); });

		DispatchOSWindowResizeEvent(g_OSWindow->GetSize());
	}

	void DispatchMouseMoveEvent(Point mousePosGlobal) {
		const auto mouseDelta = mousePosGlobal - mousePosGlobal_;
		mousePosGlobal_ = mousePosGlobal;

		MouseDragEvent mouseMoveEvent;
		mouseMoveEvent.mousePosOnCaptureLocal = mousePosOnCaptureGlobal_;
		mouseMoveEvent.mousePosLocal = mousePosGlobal;
		mouseMoveEvent.mouseDelta = mouseDelta;
		mouseMoveEvent.mouseButtonsPressedBitField = mouseButtonsPressedBitField_;

		if(mousePosGlobal == Point::Max()) {
			auto e = HoverEvent::LeaveEvent();
			for(auto* widget: hoveredWidgets_) {
				widget->OnEvent(&e);
			}
			return;
		} 
		// If we're update the hittest after rebuild, delta will be 0.0 so ingore mouse drags
		// TODO: Dispatch drag events if delta is above some threshold
		if(!capturingWidgets_.Empty()) {
			if(mouseDelta == float2::Zero()) {
				return;
			}
			for(auto* w: capturingWidgets_) {
				const auto mouseDeltaFromInitial = mousePosGlobal - mousePosOnCaptureGlobal_;
				// Convert global to local using hit test data
				const auto* parentHitData = lastHitStack_.Find(w->FindAncestorOfClass<LayoutWidget>());
				const auto  mousePosLocal = parentHitData
					? parentHitData->hitPosLocal + mouseDeltaFromInitial
					: mousePosGlobal;

				MouseDragEvent dragEvent;
				dragEvent.mousePosOnCaptureLocal = mousePosLocal - mouseDeltaFromInitial;
				dragEvent.mousePosLocal          = mousePosLocal;
				dragEvent.mouseDelta             = mouseDelta;
				w->OnEvent(&dragEvent);
			}
			return;
		}

		Widget* 		hoveredWidget = nullptr;
		Widget* 		hoveredWindow = nullptr;
		HitTestEvent	hitTest;
		auto&			hitStack = hitTest.hitStack;
		hitTest.hitPosGlobal = mousePosGlobal;
		hoveredWindow_ = nullptr;
		bool hasPopups = false;

		for(auto& [widget, layer]: std::views::reverse(widgetStack_)) {
			if(layer == Layer::Overlay) {
				continue;
			} else if(layer == Layer::Popup) { 
				hasPopups = true; 
			} else if(hasPopups) {
				continue;
			}
			// Hit test widgets recursively
			if(auto* layoutWidget = LayoutWidget::FindNearest(widget.get())) {
				layoutWidget->OnHitTest(hitTest, mousePosGlobal);
			}
			if(!hitTest.hitStack.Empty()) {
				hoveredWindow = widget.get();
				hoveredWindow_ = hoveredWindow->GetWeak();		
				break;
			}
		}
		WidgetList prevHovered = hoveredWidgets_;
		WidgetList newHovered;
		// Dispatch leave events first so that event timeline is consistent for widgets
		// I.e. if some widget receives a hover enter event it will reveive a hover leave before it's neigboor receives enter
		std::vector<Widget*> hoveredWidgets;
		{
			for(auto* widget: AncestorIterator(hitStack.TopWidget(), true)) {
				hoveredWidgets.push_back(widget);
			}
			auto e = HoverEvent::LeaveEvent();
			std::vector<Widget*> noLongerHoveredWidgets;

			for(auto* widget: prevHovered) {
				if(!std::ranges::contains(hoveredWidgets, widget)) {
					widget->OnEvent(&e);
					noLongerHoveredWidgets.push_back(widget);
				}
			};
			for(auto& w: noLongerHoveredWidgets) {
				prevHovered.Remove(w);
			}
		}
		// Dispatch mouse enter events
		if(!hitStack.Empty()) {
			auto e = HoverEvent::Normal();
			bool isHandled = false;

			for(auto* widget: hoveredWidgets) {
				const bool isMouseRegion = widget->IsA<MouseRegion>(); 
				const bool isAlways = isMouseRegion && widget->As<MouseRegion>()->ShouldAlwaysReceiveHover();

				if(isHandled && !isAlways) {
					continue;
				}
				const bool bPrevHovered = prevHovered.Contains(widget);
				e.bHoverEnter = !bPrevHovered;

				if(widget->OnEvent(&e)) {
					newHovered.Push(widget);
					++e.depth;

					if(bPrevHovered) {
						prevHovered.Remove(widget);
					}
					if(!isHandled && !isAlways) {
						isHandled = true;
					}
				}
			}
		}
		hoveredWidgets_ = newHovered;
		lastHitStack_ = hitStack;
	}

	Point GetLocalPosForWidget(Widget* widget) {
		auto out = mousePosGlobal_;
		if(auto* layoutParent = widget->FindAncestorOfClass<LayoutWidget>()) {
			if(auto* hitData = lastHitStack_.Find(layoutParent)) {
				out = hitData->hitPosLocal;
			}
		}
		return out;
	}

	void DispatchMouseButtonEvent(KeyCode button, bool bPressed) {
		bPressed ? ++mouseButtonHeldNum_ : --mouseButtonHeldNum_;
		bPressed ? mouseButtonsPressedBitField_ |= (MouseButtonMask)button
			: mouseButtonsPressedBitField_ &= ~(MouseButtonMask)button;

  		// When pressed first time from default
		if(bPressed && mouseButtonHeldNum_ == 1) {
			pressedMouseButton_ = (MouseButton)button;

			MouseButtonEvent event;
			event.mousePosGlobal = mousePosGlobal_;
			event.mousePosLocal = mousePosGlobal_;
			event.isPressed = bPressed;
			event.button = (MouseButton)button;
			bool bHandled = false;

			for(auto it = lastHitStack_.begin(); it != lastHitStack_.end(); ++it) {
				auto& [layoutWidget, pos] = *it;
				// In case widget has been deleted between events
				if(!layoutWidget) continue;
				event.mousePosLocal = mousePosGlobal_;

				if(auto parentIt = it + 1; parentIt != lastHitStack_.end()) {
					event.mousePosLocal = parentIt->hitPosLocal;
				}
				auto widget = layoutWidget->GetWeakAs<Widget>();

				do {
					// Mouse regions have an option to always receive events, so they cannot block them from propagating
					const auto bMouseRegion = widget->IsA<MouseRegion>(); 
					const auto bAlways = bMouseRegion && widget->As<MouseRegion>()->ShouldAlwaysReceiveHover();

					if(!bHandled || bAlways) {
						if(widget->OnEvent(&event)) {
							// Widget could be deleted on this event
							if(!widget) break;
							capturingWidgets_.Push(widget.Get());
							
							if(!bHandled && !bAlways) {
								bHandled = true;
								mousePosOnCaptureGlobal_ = mousePosGlobal_;
							}
						}
					}
					widget = widget->GetParent() 
						? widget->GetParent()->GetWeakAs<Widget>()
						: nullptr;
				} while(widget && !widget->IsA<LayoutWidget>());
			}
			return;
		}

		if(!capturingWidgets_.Empty() && !bPressed && (MouseButton)button == pressedMouseButton_) {
			MouseButtonEvent e;
			e.isPressed = bPressed;
			e.button = (MouseButton)button;
			e.mousePosGlobal = mousePosGlobal_;
			e.mousePosLocal = mousePosGlobal_;

			for(auto* w: capturingWidgets_) {
				e.mousePosLocal = GetLocalPosForWidget(w);
				w->OnEvent(&e);
			}
			capturingWidgets_.Clear();
			mousePosOnCaptureGlobal_ = Point::Max();
			pressedMouseButton_ = MouseButton::None;
		}
	}

	void DispatchKeyEvent(KeyCode button, bool bPressed) {
		if(button == KeyCode::KEY_LEFT_SHIFT) modifiersState_[LeftShift] = !modifiersState_[LeftShift];
		if(button == KeyCode::KEY_RIGHT_SHIFT) modifiersState_[RightShift] = !modifiersState_[RightShift];

		if(button == KeyCode::KEY_LEFT_CONTROL) modifiersState_[LeftControl] = !modifiersState_[LeftControl];
		if(button == KeyCode::KEY_RIGHT_CONTROL) modifiersState_[RightControl] = !modifiersState_[RightControl];

		if(button == KeyCode::KEY_LEFT_ALT) modifiersState_[LeftAlt] = !modifiersState_[LeftAlt];
		if(button == KeyCode::KEY_RIGHT_ALT) modifiersState_[RightAlt] = !modifiersState_[RightAlt];

		if(button == KeyCode::KEY_CAPS_LOCK) modifiersState_[CapsLock] = !modifiersState_[CapsLock];

		if(button == KeyCode::KEY_P && bPressed) {
			for(auto& [window, layer]: std::views::reverse(widgetStack_)) {
				LogWidgetTree(window.get());
			}
			return;
		} 
		
		if(button == KeyCode::KEY_D && bPressed) {
			bDrawDebugInfo_ = !bDrawDebugInfo_;
			debugOverlay_->isVisible_ = !debugOverlay_->isVisible_;
			debugOverlay_->MarkNeedsRebuild();
			return;
		}

		if(button == KeyCode::KEY_L && bPressed) {
			bDrawDebugLayout_ = !bDrawDebugLayout_;
			return;
		}
		
		if(button == KeyCode::KEY_C && bPressed) {
			bDrawDebugClipRects_ = !bDrawDebugClipRects_;
			return;
		}
		
		if(button == KeyCode::KEY_H && bPressed) {
			LOGF(Info, "{}", PrintContext());
			return;
		}
	}

	void DispatchMouseScrollEvent(float scroll) {
		if (!hoveredWindow_) {
			return;
		}
		MouseScrollEvent e;
		e.scrollDelta = {0.f, scroll};

		for(auto* mouseRegion: AncestorIterator<MouseRegion>(lastHitStack_.TopWidget(), true)) {
			if(mouseRegion->OnEvent(&e)) {
				return;
			}
		}
	}

	void DispatchOSWindowResizeEvent(float2 windowSize) {
		// Ignore minimized state for now
		if(windowSize == float2()) {
			return;
		}
		LayoutConstraints layoutEvent;
		layoutEvent.rect = Rect(windowSize);

		for(auto& [window, layer] : widgetStack_) {
			UpdateLayout(window.get());
		}
		g_Renderer->ResizeFramebuffers(windowSize);
	}

	void DispatchCharInputEvent(wchar_t ch) {}

	// Prints whole application context into a string
	std::string PrintContext() {
		std::string out;
		auto sb = util::StringBuilder(&out);
		sb.Line("Application context:\n");

		sb.Line("Opened window count: {}", widgetStack_.size());
		sb.Line("Timers count: {}", timers_.Size());
		sb.Line("Hovered window: {}", hoveredWindow_ ? hoveredWindow_->GetDebugID() : "");
		sb.Line("Hovered widget: {}", !hoveredWidgets_.Empty() ? hoveredWidgets_.Print() : "");
		sb.Line("Mouse pos local: {}", hoveredWidgets_.Top() ? GetLocalPosForWidget(hoveredWidgets_.Top()) : mousePosGlobal_);
		sb.Line("Mouse pos global: {}", mousePosGlobal_ == Point::Max() ? "OUTSIDE" : std::format("{}", mousePosGlobal_));
		sb.Line("Capturing mouse widgets: {}", !capturingWidgets_.Empty() ? capturingWidgets_.Print() : "");
		sb.Line();
		
		for(auto it = widgetStack_.rbegin(); it != widgetStack_.rend(); ++it) {
			PropertyArchive archive;
			DebugLogEvent onDebugLog;
			onDebugLog.archive = &archive;

			it->widget->OnEvent(&onDebugLog);

			std::deque<PropertyArchive::ObjectID> parentIDStack;
			std::stringstream ss;

			auto printIndent = [&](size_t indent) {
				if(!indent) return;
				ss << "    ";
				--indent;

				for(; indent; --indent) {
					ss << "|   ";
				}
			};

			auto visitor = [&](PropertyArchive::Object& object)->bool {
				const auto parentID = object.parent_ ? object.parent_->objectID_ : 0;

				if(!parentID) {
					parentIDStack.clear();
				} else if(parentID != parentIDStack.back()) {
					// Unwind intil the parent is found
					for(auto stackParentID = parentIDStack.back();
						stackParentID != parentID;
						parentIDStack.pop_back(), stackParentID = parentIDStack.back());
				}
				parentIDStack.push_back(object.objectID_);
				auto indent = parentIDStack.size();

				printIndent(indent); ss << '\n';
				printIndent(indent - 1); ss << "|-> " << object.debugName_ << ":\n";

				for(auto& property : object.properties_) {
					printIndent(indent);
					ss << std::format("{}: {}", property.Name, property.Value) << '\n';
				}
				return true;
			};
			archive.VisitRecursively(visitor);
			sb.Line(ss.str());
			sb.Line();
		}
		return out;
	}

	void LogWidgetTree(Widget* window) {
		PropertyArchive archive;
		DebugLogEvent onDebugLog;
		onDebugLog.archive = &archive;

		window->OnEvent(&onDebugLog);

		std::deque<PropertyArchive::ObjectID> parentIDStack;
		std::stringstream ss;

		auto printIndent = [&](size_t indent) {
			if(!indent) return;
			ss << "    ";
			--indent;

			for(; indent; --indent) {
				ss << "|   ";
			}
		};
		archive.VisitRecursively([&](PropertyArchive::Object& object)->bool {
			const auto parentID = object.parent_ ? object.parent_->objectID_ : 0;

			if(!parentID) {
				parentIDStack.clear();
			} else if(parentID != parentIDStack.back()) {
				// Unwind intil the parent is found
				for(auto stackParentID = parentIDStack.back();
					stackParentID != parentID;
					parentIDStack.pop_back(), stackParentID = parentIDStack.back());
			}
			parentIDStack.push_back(object.objectID_);
			auto indent = parentIDStack.size();

			printIndent(indent); ss << '\n';
			printIndent(indent - 1); ss << "|-> " << object.debugName_ << ":\n";

			for(auto& property : object.properties_) {
				printIndent(indent);
				ss << std::format("{}: {}", property.Name, property.Value) << '\n';
			}
			return true;
		});
		LOGF(Info, "Widget tree: \n{}", ss.str());
	}

	std::string PrintAncestors(Widget* widget) {
		std::string out;
		PropertyArchive ar;
		for(auto* w: AncestorIterator(widget, true)) {
			ar.PushObject(w->GetDebugID(), w, nullptr);
			w->DebugSerialize(ar);
		}
		util::StringBuilder sb(&out);
		sb.Line(PrintPropertyArchive(ar, true));
		return out;
	}

	std::string PrintPropertyArchive(const PropertyArchive& ar, bool reversed = false) {
		std::string out;
		util::StringBuilder sb(&out);

		auto print = [&](PropertyArchive::Object& object) {
			sb.Line(object.debugName_);
			sb.PushIndent();

			for(auto& property: object.properties_) {
				sb.Line("{}: {}", property.Name, property.Value);
			}
			sb.PopIndent();
		};
		if(reversed) {
			for(auto it = ar.rootObjects_.begin(); it != ar.rootObjects_.end(); ++it) {
				print(**it);
			}
		} else {
			for(auto it = ar.rootObjects_.rbegin(); it != ar.rootObjects_.rend(); ++it) {
				print(**it);
			}
		}
		return out;
	}

	std::string PrintHitStack(u32 indent = 1) {
		std::string out;
		util::StringBuilder sb(&out);
		sb.PushIndent(indent);
		PropertyArchive ar;

		for(auto& hitData : lastHitStack_) {
			if(!hitData.widget) continue;
			ar.PushObject(hitData.widget->GetDebugID(), *hitData.widget, nullptr);
			hitData.widget->DebugSerialize(ar);
		}
		for(auto& object: ar.rootObjects_) {
			sb.Line(object->debugName_);
			sb.PushIndent();

			for(auto& property : object->properties_) {
				sb.Line("{}: {}", property.Name, property.Value);
			}
			sb.PopIndent();
		}
		return out;
	}

	// Update cached widgets for mouse capture and hovering
	void ResetState() {
		hoveredWindow_ = nullptr;
		hoveredWidgets_.Clear();
		capturingWidgets_.Clear();
		bResetState_ = true;
	}
	
	// Rebuild font of the theme if needed
	void RebuildFonts() {
		std::vector<Font*> fonts;
		theme_->RasterizeFonts(&fonts);

		for(auto& font : fonts) {
			if(font->NeedsRebuild()) {
				Image fontImageAtlas;
				font->Build(&fontImageAtlas);
				auto oldTexture = (TextureHandle)font->GetAtlasTexture();
				TextureHandle fontTexture = g_Renderer->CreateTexture(fontImageAtlas);

				font->SetAtlasTexture(fontTexture);
				g_Renderer->DeleteTexture(oldTexture);
			}
		}
	}

	// We will iterate a subtree manually and handle nesting and visibility
	// Possibly opens a possibility to cache draw commands
	// Also we will handle clip rects nesting
	void DrawWindow(Widget* window, DrawList* rendererDrawlist, Theme* theme) {
		CanvasImpl canvas(rendererDrawlist);

		DrawEvent drawEvent;
		drawEvent.canvas = &canvas;
		drawEvent.theme = theme;

		std::function<VisitResult(Widget*)> drawFunc;
		// Draw children recursively using DFS
		drawFunc = [&](Widget* widget) {
			if(auto* layout = widget->As<LayoutWidget>()) {
				const auto rect = layout->GetRect();
				Assertf(!rect.Empty(), 
						"A widget with 0 size encountered in DrawWindow().\n"
						"Widget: {}\nAncestors:\n{}",
						layout->GetDebugID(),
						PrintAncestors(layout));

				widget->OnEvent(&drawEvent);

				if(bDrawDebugClipRects_ && canvas.HasClipRect()) {
					if(lastHitStack_.TopWidget() == layout) {
						canvas.DrawClipRect(true, colors::red.MultiplyAlpha(0.3f));
					} else {
						canvas.DrawClipRect(false, colors::red);
					}
				}
				if(bDrawDebugLayout_){
					if(lastHitStack_.TopWidget() == layout) {
						canvas.DrawRect(layout->GetRect().Expand(-1), colors::green.MultiplyAlpha(0.3f), true);
					} else {
						canvas.DrawRect(layout->GetRect().Expand(-1), colors::green, false);
					}
				}
				canvas.PushTransform(layout->GetTransform());
				canvas.SaveContext();
				widget->VisitChildren(drawFunc);
				canvas.RestoreContext();
			} else {
				widget->VisitChildren(drawFunc);
			}
			return VisitResult::Continue();
		};
		drawFunc(window);
		canvas.ClearContext();
	}

	void UpdateLayout(Widget* widget) {
		LayoutWidget* layoutWidget = nullptr;

		for(Widget* ancestor: AncestorIterator(widget)) {
			if(auto* l = ancestor->As<LayoutWidget>()) {
				const auto axisMode = l->GetAxisMode();
				const auto layoutDependsOnChildren =
					axisMode.x == AxisMode::Shrink || axisMode.y == AxisMode::Shrink;

				if(!layoutDependsOnChildren) {
					layoutWidget = l;
					break;
				}
			}
		}
		if(!layoutWidget) {
			layoutWidget = LayoutWidget::FindNearest(widget);
		}
		if(!layoutWidget) {
			return;
		}
		// Update layout of children recursively starting from this widget
		LayoutConstraints e;
		auto* parentLayout = layoutWidget->FindAncestorOfClass<LayoutWidget>();
		const bool isRoot = parentLayout == nullptr;

		if(isRoot) {
			e.rect = Rect(float2(0), g_OSWindow->GetSize());
		} else {
			const auto layoutInfo = *layoutWidget->GetLayoutStyle();
			e.parent = parentLayout;
			e.rect = Rect(
                layoutWidget->GetOrigin() - layoutInfo.margins.TL(),
                layoutWidget->GetSize() + layoutInfo.margins.Size()
			);
		}
		layoutWidget->OnLayout(e);
		layoutWidget->OnPostLayout();
	}

	// Recursively calls Build() on requsted widgets in the dirty list
	// If the widget in the new tree has the same class as position as the widget in the old tree
	// 		the old widget is updated with the new one and all raw pointers and weak pointers to it remain valid
	//		also hovered and pressed state is preserved for such widget
	// TODO:
	// - Take widget ids into account
	// - Add "BuildStackTrace" for build debugging
	// - Index of the child being iterated
	void RebuildDirtyWidgets() {
		if(dirtyWidgets_.empty()) {
			return;
		}
		StatefulWidget* requestedWidget = nullptr;

		std::function<void(StatefulWidget*)> build;
		build = [&](StatefulWidget* widget) {
			auto* state = widget->GetState();

			if(!state) {
				auto s = PrintAncestors(widget);
				Assertf(state != nullptr, 
						"Stafeful widget without state found during Build. Ancestor tree:\n{}\nContext:\n{}", 
						s, 
						PrintContext());
			}
			auto* oldWidget = state->GetWidget();

			if(!widget->NeedsRebuild()) {
				if(!oldWidget || !oldWidget->GetChild()) {
					auto ancestors = PrintAncestors(widget);
					Assertf(oldWidget != nullptr, 
							"A widget with clear state and no old children is found while building the widget {}. Ancestor tree:\n{}\nContext:\n{}", 
							requestedWidget->GetDebugID(),
							ancestors,
							PrintContext());
				}
				if(widget != requestedWidget) {
					std::unique_ptr<Widget> oldChild = oldWidget->OrphanChild();
					if(oldChild) {
						widget->Parent(std::move(oldChild));
					}
					widget->RebindToState();
				}
				return;
			}
			// Start diffing the new tree with the old one
			// There are 3 posible results:
			// 		Update: The old widget is updated with the new one and stays in the tree
			// 		Swap: The old widget is swapped with the new one
			//		Discard: If widget types are different, whole old tree is discarded and replaced with the new one
			std::function<void(Widget*, Widget*, Widget*)> diff;
			diff = [&](Widget* oldWidget, Widget* newWidget, Widget* parent) {
				Widget* newParent = nullptr;

				if(!newWidget) {
					std::unique_ptr<Widget> discardWidget = parent->Orphan(oldWidget);
					return;
				}
				if(auto* stateful = newWidget->As<StatefulWidget>()) {
					build(stateful);	
					return;
				}
				if(!oldWidget) {
					if(newWidget->GetParent() != parent) {
						parent->Parent(newWidget->GetParent()->Orphan(newWidget));		
					}
					newWidget->VisitChildrenRecursively([&build](Widget* child) {
						if(auto* w = child->As<StatefulWidget>()) {
							build(w);
						}
						return VisitResult::Continue();
					});
					return;
				}
				// If we continue diffing children this will own the widget for diff
				// and will be deleted on the way back
				std::unique_ptr<Widget> pendingDelete;

				const bool discard = oldWidget->GetClass() != newWidget->GetClass();
				if(discard) {
					if(newWidget->GetParent() != parent) {
						std::unique_ptr<Widget> discardWidget = parent->Orphan(oldWidget);
						parent->Parent(newWidget->GetParent()->Orphan(newWidget));
					}
					return;

				} else if(oldWidget->UpdateWith(newWidget)) {
					newParent = oldWidget;
					if(oldWidget->GetParent() != parent) {
						pendingDelete = std::move(newWidget->GetParent()->Orphan(newWidget));
						parent->Parent(oldWidget->GetParent()->Orphan(oldWidget));
					}
				} else {
					newParent = newWidget;
					if(newWidget->GetParent() != parent) {
						pendingDelete = std::move(oldWidget->GetParent()->Orphan(oldWidget));
						parent->Parent(newWidget->GetParent()->Orphan(newWidget));
					}
				}
				// Diff children
				std::vector<Widget*> oldWidgets;
				std::vector<Widget*> newWidgets;

				oldWidget->VisitChildren([&](Widget* child) {
					oldWidgets.push_back(child);
					return VisitResult::Continue();
				});
				newWidget->VisitChildren([&](Widget* child) {
					newWidgets.push_back(child);
					return VisitResult::Continue();
				});

				for(size_t index = 0; index < newWidgets.size(); ++index) {
					auto* oldWidget = index < oldWidgets.size() ? oldWidgets[index] : nullptr;
					diff(oldWidget, newWidgets[index], newParent);
				}
				if(parent == oldWidget->GetParent() && oldWidgets.size() > newWidgets.size()) {
					auto oldWidgetsToDeleteNum = oldWidgets.size() - newWidgets.size();

					for(auto i = oldWidgetsToDeleteNum; i < oldWidgets.size(); ++i) {
						std::unique_ptr<Widget> toDeleteWidget = parent->Orphan(oldWidgets[i]);
					}
				}
			};
			
			std::unique_ptr<Widget> newTree = widget->Build();
			if(!newTree) {
				if(widget == requestedWidget) {
					std::unique_ptr<Widget> discard;
					discard = widget->OrphanChild();
				} else {
					widget->RebindToState();
				}
				return;
			}
			if(widget == requestedWidget) {
				std::unique_ptr<Widget> oldChild;
				oldChild = widget->OrphanChild();
				widget->Parent(std::move(newTree));
				auto* newChild = widget->GetChild();

				if(!oldChild) {
					diff(nullptr, newChild, widget);
				} else {
					if(oldChild->GetClass() != newChild->GetClass()) {
						diff(nullptr, newChild, widget);

					} else if(oldChild->UpdateWith(newChild)) {
						std::unique_ptr<Widget> discard = widget->OrphanChild();
						widget->Parent(std::move(oldChild));
						diff(widget->GetChild(), discard.get(), widget);

					} else {
						diff(oldChild.get(), newChild, widget);	
					}
				}
			} else {
				widget->Parent(std::move(newTree));
				auto* old = oldWidget ? oldWidget->GetChild() : nullptr;
				diff(old, widget->GetChild(), widget);
			}
			widget->RebindToState();
			return;
		};

		for(auto& widget: dirtyWidgets_) {
			if(widget) {
				requestedWidget = widget.Get();
				build(widget.Get());
			}
		}
	}

	void UpdateDebugOverlay() {
		if(!bDrawDebugInfo_) {
			return;
		}
		std::string str;
		auto sb = util::StringBuilder(&str);

		sb.Line("Frame time: {:5.2f}ms", lastFrameTimeMs_);
		sb.Line("Opened window count: {}", widgetStack_.size());
		sb.Line("Timers count: {}", timers_.Size());
		sb.Line("Hovered window: {}", hoveredWindow_ ? hoveredWindow_->GetDebugID() : "");
		sb.Line("Hovered widget: {}", !hoveredWidgets_.Empty() ? hoveredWidgets_.Print() : "");
		sb.Line("Mouse pos local: {}", [&]()->std::string {
			if(mousePosGlobal_ == Point::Max()) {
				return "OUTSIZE";
			}
			if(auto* topHovered = hoveredWidgets_.Top()) {
				return std::format("{}", GetLocalPosForWidget(topHovered));
			}
			return std::format("{}", mousePosGlobal_);
		}());
		sb.Line("Mouse pos global: {}", mousePosGlobal_ == Point::Max() ? "OUTSIDE" : std::format("{}", mousePosGlobal_));
		sb.Line("Capturing mouse widgets: {}", !capturingWidgets_.Empty() ? capturingWidgets_.Print() : "");
		sb.Line();

		if(hoveredWindow_) {
			sb.Line("{} Hit stack: ", hoveredWindow_->GetDebugID());
			sb.Line(PrintHitStack());
		}
		debugOverlay_->SetText(str);
	}

public:

	bool Tick() final {
		const auto frameStartTimePoint = std::chrono::high_resolution_clock::now();
		// Rebuild fonts if needed for different size
		RebuildFonts();

		if(bResetState_) {
			DispatchMouseMoveEvent(mousePosGlobal_);
			bResetState_ = false;
		}

		if(!g_OSWindow->PollEvents()) {
			return false;
		}
		timers_.Tick();
		RebuildDirtyWidgets();

		// Update layout
		// Copy because widgets can request rebuild in OnPostLayout()
		// based on layout size. For examle Scrollbar
		auto pendingUpdateLayout = dirtyWidgets_;
		dirtyWidgets_.clear();

		for(auto& widget: pendingUpdateLayout) {
			if(widget) {
				UpdateLayout(widget.Get());
			}
		}
		// Update hittest
		DispatchMouseMoveEvent(mousePosGlobal_);

		UpdateDebugOverlay();

		// Draw
		g_Renderer->ResetDrawLists();
		auto* frameDrawList = g_Renderer->GetFrameDrawList();
		frameDrawList->PushFont(theme_->GetDefaultFont(), g_DefaultFontSize);
		for(auto& [window, layer] : widgetStack_) {
			DrawWindow(window.get(), frameDrawList, theme_.get());
		}

		const auto frameEndTimePoint = std::chrono::high_resolution_clock::now();
		++frameNum_;
		lastFrameTimeMs_ = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTimePoint - frameStartTimePoint).count() / 1000.f;

		// Kick actual rendering
		// TODO: make async with two contexts
		// so that we can start updating next frame while previous is rendering
		g_Renderer->RenderFrame(true);
		return true;
	}
	
	std::unique_ptr<Widget> Orphan(Widget* widget) override {
		// Remove child without deletion
		for(auto it = widgetStack_.begin(); it != widgetStack_.end(); ++it) {
			if(it->widget.get() == widget) {
				auto out = std::move(it->widget);
				widgetStack_.erase(it);
				return out;
			}
		}
		return {};
	}
	
	void RequestRebuild(StatefulWidget* widget) override {
		auto it = std::ranges::find_if(
			dirtyWidgets_, 
			[&](const auto& w) {  
				return w && w.Get() == widget;
			}
		);
		if(it == dirtyWidgets_.end()) {
			dirtyWidgets_.push_back(widget->GetWeak());
		}
	}

	void BringToFront(Widget* widget) override {
		using Iterator = decltype(widgetStack_)::iterator;
		auto found = false;
		// Bring the child just below the overlay layer
		for(auto it = widgetStack_.begin(); it != widgetStack_.end(); ++it) {
			if(!found && it->widget.get() == widget) {
				found = true;
			} 
			if(found) {
				auto nextItem = ++Iterator(it);
				if(nextItem != widgetStack_.end() && nextItem->layer != Layer::Overlay) {
					std::swap(it->widget, nextItem->widget);
				}
			}			
		}
	}

	void Parent(std::unique_ptr<Widget>&& widget, Layer layer) override {
		auto* w = widget.get();
		// Insert the widget based on layer
		// Call OnParented()
		if(layer == Layer::Overlay) {
			widgetStack_.emplace_back(RootWidget(std::move(widget), layer));

		} else if(layer == Layer::Background) {
			widgetStack_.emplace_front(RootWidget(std::move(widget), layer));

		} else {
			for(auto it = widgetStack_.begin(); it != widgetStack_.end(); ++it) {
				// Insert the window before overlays
				if(it->layer == Layer::Overlay) {
					widgetStack_.insert(it, RootWidget(std::move(widget), layer));
					break;
				}
			}
		}
		w->OnParented(this);
		auto* statefulChild = w->As<StatefulWidget>() 
			? w->As<StatefulWidget>() 
			: w->FindChildOfClass<StatefulWidget>();

		if(statefulChild) {
			RequestRebuild(statefulChild);	
		} else {
			UpdateLayout(w);
		}
	}

	void RequestFocus(FocusNode* node) {
		// Create new focus chain
		// Compare with precious chain
		// Dispatch unfocus events
		// Edge cases:
		// 		Requested node is already focused
		WidgetList& oldChain = focusedWidgets_;
		WidgetList  newChain;

		for(auto* focusedAncestor: AncestorIterator<Focused>(node->GetWidget(), true)) {
			newChain.Push(focusedAncestor);
		}
		LOGF(Verbose, "Focus request is handled.\nOld chain:{}\nNew chain:{}", oldChain.Print(), newChain.Print());

		DiffListsSimple(
			oldChain, 
			newChain,
			[&](Widget* oldFocused) {
				oldFocused->As<Focused>()->OnFocusChanged(false);
			},
			[&](Widget* newFocused) {
				newFocused->As<Focused>()->OnFocusChanged(true);
			}
		);
		focusedWidgets_ = newChain;
	}

	bool OnEvent(IEvent* e) {
		return false;
	}

	// TODO: rework
	void Shutdown() final {}

	Theme* GetTheme() final {
		Assertm(theme_, "A theme should be set before creating widgets");
		return theme_.get();
	}

	FrameState GetFrameStateImpl() {
		auto state = FrameState();
		state.keyModifiersState = modifiersState_;
		state.mouseButtonHeldNum = mouseButtonHeldNum_;
		state.mouseButtonsPressedBitField = mouseButtonsPressedBitField_;
		state.mousePosGlobal = mousePosGlobal_;
		state.mousePosOnCaptureGlobal = mousePosOnCaptureGlobal_;
		state.windowSize = g_OSWindow->GetSize();
		state.theme = theme_.get();
		return state;
	}

	TimerHandle	AddTimer(Object* object, const TimerCallback& callback, u64 periodMs) override {
		return timers_.AddTimer(object, callback, periodMs);
	}

	void RemoveTimer(TimerHandle handle) override {
		timers_.RemoveTimer(handle);
	}

private:
	u64						frameNum_ = 0;
	float					lastFrameTimeMs_ = 0;
	
	// Whether the native window is minimized
	bool					bMinimized_ = false;
	bool					bResetState_ = false;

	// Position of the mouse cursor when button has been pressed
	Point					mousePosOnCaptureGlobal_;
	Point					mousePosGlobal_;

	// Number of buttons currently held
	u8						mouseButtonHeldNum_ = 0;
	// We allow only one button to be pressed simultaniously
	MouseButton				pressedMouseButton_ = MouseButton::None;
	MouseButtonMask			mouseButtonsPressedBitField_ = MouseButtonMask::None;
	KeyModifiersArray		modifiersState_{false};

	// Layout widgets that are hit by mouse cursor
	HitStack				lastHitStack_;
	WidgetList				hoveredWidgets_;
	WidgetList				capturingWidgets_;
	WidgetList				focusedWidgets_;

	WeakPtr<Widget>			hoveredWindow_;
	
	// Draw hitstack and mouse pos
	std::unique_ptr<DebugOverlayWindow>	
							debugOverlay_;

	bool					bDrawDebugInfo_ = true;
	bool					bDrawDebugLayout_ = false;
	bool					bDrawDebugClipRects_ = false;

	// Stack of windows, bottom are background windows and top are overlay windows
	// Other windows in the middle
	struct RootWidget {
		std::unique_ptr<Widget> widget;
		Layer 					layer;
	};
	std::list<RootWidget>	widgetStack_;

	// Widgets which state has been changed and need rebuilding
	// FIXME: It should contain only one widget from a branch
	// Only the topmost ancestor, because when the topmost is rebuild 
	// another widgets will be updated too, and pointers will become stale
	// We will use a weakptr as a workaround
	std::vector<WeakPtr<StatefulWidget>> dirtyWidgets_;

	TimerList				timers_;
	std::unique_ptr<Theme>	theme_;

};

ui::Application* ui::Application::Create(std::string_view windowTitle, u32 width, u32 height) {
	if(!g_OSWindow) { g_OSWindow = INativeWindow::createWindow(windowTitle, width, height); }
	if(!g_Application) g_Application = new ApplicationImpl();
	g_Application->Init();
	return g_Application;
}

ui::Application* ui::Application::Get() {
	return g_Application;
}

ui::FrameState ui::Application::GetState() {
	return g_Application->GetFrameStateImpl();
}

} // namespace ui