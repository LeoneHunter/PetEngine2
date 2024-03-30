#include "framework.h"
#include "widgets.h"
#include "runtime/platform/native_window.h"
#include "runtime/system/renderer/ui_renderer.h"
#include "runtime/system/job_dispatcher.h"

#include "thirdparty/optik/include/optick.config.h"
#include "thirdparty/optik/include/optick.h"

#include <stack>

#define GAVAUI_BEGIN namespace UI {
#define GAVAUI_END }

GAVAUI_BEGIN

using Timer = util::Timer<std::chrono::milliseconds>;
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
		if(!context.clipRect.Empty()) {
			drawList->PopClipRect();
		}	
		clipRect = Transform(clipRect);
		context.clipRect = clipRect;
		drawList->PushClipRect(clipRect);
	}

	void DrawClipRect(bool bFilled, Color color) {
		if(!context.clipRect.Empty()) {
			drawList->PopClipRect();

			if(bFilled) {
				drawList->DrawRectFilled(context.clipRect, color);
			} else {
				drawList->DrawRect(context.clipRect, color);
			}
			drawList->PushClipRect(context.clipRect);
		}
	}

	void PushTransform(float2 transform) {
		context.transform = transform;
	}

	void SaveContext() {
		contextStack.push_back(context);
		cummulativeTransform += context.transform;
		context.transform = {};
		context.clipRect = {};
	}

	void RestoreContext() {
		Assert(!contextStack.empty());
		cummulativeTransform -= contextStack.back().transform;

		if(!contextStack.back().clipRect.Empty()) {
			drawList->PopClipRect();
		}
		contextStack.pop_back();
	}

	Point Transform(Point point) {
		return point + cummulativeTransform + context.transform;
	}

	Rect Transform(Rect rect) {
		return Rect(rect).Translate(cummulativeTransform + context.transform);
	}

private:
	RendererDrawlist* 	 drawList = nullptr;

	struct Context {
		Point transform;
		Rect  clipRect;
	};
	Context              context;
	std::vector<Context> contextStack;
	float2               cummulativeTransform;
};


/*
* Helper adapter to store widget state between frames
*/
class WidgetList {
public:

	WidgetList() {
		stack.reserve(10);
	}

	void Push(Widget* item) {
		stack.push_back(item->GetWeak());
	}

	void ForEach(const std::function<void(Widget*)>& fn) {
		for(auto& item: stack) {
			if(!item) continue;
			fn(item.Get());
		}
	}

	constexpr void Remove(Widget* item) {
		for(auto it = stack.begin(); it != stack.end(); ++it) {
			if(*it == item) {
				stack.erase(it);
				break;
			}
		}
	}

	constexpr bool Contains(Widget* item) {
		for(auto it = stack.begin(); it != stack.end(); ++it) {
			if(*it == item) {
				return true;
			}
		}
		return false;
	}

	constexpr void Clear() { stack.clear(); }
	constexpr bool Empty() const { return stack.empty(); }	

	Widget* Top() {
		return stack.empty() 
			? nullptr 
			: stack.front().GetChecked(); 
	}

	const Widget* Top() const {
		return stack.empty() 
			? nullptr 
			: stack.front().GetChecked(); 
	}

	std::string Print() const {
		std::string out;
		util::StringBuilder sb(&out);
		sb.PushIndent();
		sb.Line();

		for(auto& w: stack) {
			if(!w) continue;
			sb.Line(w->GetDebugID());
		}
		return out;
	}

private:
	std::vector<WeakPtr<Widget>> stack;
};



/*
* Overlay that is drawn over root window
*/
class DebugOverlayWindow: public WidgetState {
public:

	std::unique_ptr<Widget> Build() {
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

	std::string text_;
};


/*
* Top object that handles input events and drawing
*/
class ApplicationImpl final:
	public UI::Application,
	public Widget {
public:

	ApplicationImpl(): Widget("Application") {}

	void Init() {
		OPTICK_EVENT("UI Init");
		g_Renderer = CreateRendererDX12();
		g_Renderer->Init(g_OSWindow);

		theme_.reset(new Theme());

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

		if(mousePosGlobal == NOPOINT) {
			auto e = HoverEvent::LeaveEvent();
			hoveredWidgets_.ForEach([&](Widget* widget) {
				widget->OnEvent(&e);
			});
			return;
		} 
		
		if(!capturingWidgets_.Empty()) {
			capturingWidgets_.ForEach([&](Widget* w) {
				const auto mouseDeltaFromInitial = mousePosGlobal - mousePosOnCaptureGlobal_;
				// Convert global to local using hit test data
				const auto* parentHitData = lastHitStack_.Find(w->FindParentOfClass<LayoutWidget>());
				const auto  mousePosLocal = parentHitData
					? parentHitData->hitPosLocal + mouseDeltaFromInitial
					: mousePosGlobal;

				MouseDragEvent dragEvent;
				dragEvent.mousePosOnCaptureLocal = mousePosLocal - mouseDeltaFromInitial;
				dragEvent.mousePosLocal          = mousePosLocal;
				dragEvent.mouseDelta             = mouseDelta;
				w->OnEvent(&dragEvent);
			});
			return;
		}

		Widget* 		hoveredWidget = nullptr;
		Widget* 		hoveredWindow = nullptr;
		HitTestEvent	hitTest;
		auto&			hitStack = hitTest.hitStack;
		hitTest.hitPosGlobal = mousePosGlobal;
		hoveredWindow_ = nullptr;
		bool bHasPopups = false;

		for(auto it = widgetStack_.rbegin(); it != widgetStack_.rend(); ++it) {
			auto& [widget, layer] = *it;

			if(layer == Layer::Overlay) {
				continue;
			} else if(layer == Layer::Popup) { 
				bHasPopups = true; 
			} else if(bHasPopups) {
				continue;
			}
			auto bHovered = false;

			if(auto* layoutWidget = LayoutWidget::FindNearest(widget.get())) {
				bHovered = layoutWidget->OnEvent(&hitTest);
			}
			if(bHovered) {
				hoveredWindow = widget.get();
				hoveredWindow_ = hoveredWindow->GetWeak();		
				break;
			}
		}
		WidgetList prevHovered = hoveredWidgets_;
		WidgetList newHovered;

		// Dispatch leave events first so that event timeline is consistent for widgets
		// I.e. if some widget receives a hover enter event it will reveive a hover leave before it's neigboor receives enter
		{
			std::vector<Widget*> hoveredWidgets;
			for(Widget* widget = hitStack.TopWidget(); 
				widget; 
				widget = widget->GetParent()) { 
				hoveredWidgets.push_back(widget);
			}
			auto e = HoverEvent::LeaveEvent();
			std::vector<Widget*> noLongerHoveredWidgets;

			prevHovered.ForEach([&](Widget* widget) {
				auto contains = std::ranges::find(hoveredWidgets, widget) != hoveredWidgets.end();
				if(!contains) {
					widget->OnEvent(&e);
					noLongerHoveredWidgets.push_back(widget);
				}
			});
			for(auto& w: noLongerHoveredWidgets) {
				prevHovered.Remove(w);
			}
		}

		// Dispatch mouse enter events
		if(!hitStack.Empty()) {
			auto e = HoverEvent::Normal();
			bool bHandled = false;

			for(Widget* widget = hitStack.TopWidget(); 
				widget; 
				widget = widget->GetParent()) {

				const auto bMouseRegion = widget->IsA<MouseRegion>(); 
				const auto bAlways = bMouseRegion && widget->As<MouseRegion>()->ShouldAlwaysReceiveHover();

				if(bHandled && !bAlways)
					continue;
				const bool bPrevHovered = prevHovered.Contains(widget);
				e.bHoverEnter = !bPrevHovered;

				if(widget->OnEvent(&e)) {
					newHovered.Push(widget);
					++e.depth;

					if(bPrevHovered)
						prevHovered.Remove(widget);

					if(!bHandled && !bAlways)
						bHandled = true;
				}
			}
		}
		hoveredWidgets_ = newHovered;
		lastHitStack_ = hitStack;
	}

	Point GetLocalPosForWidget(Widget* widget) {
		auto out = mousePosGlobal_;
		
		if(auto* layoutParent = widget->FindParentOfClass<LayoutWidget>()) {
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
			event.bPressed = bPressed;
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
						LOGF(Verbose, "Button event {} dispatched to {}", event.GetDebugID(), widget->GetDebugID());
						LOGF(Verbose, "Local mouse pos: {}", event.mousePosLocal);

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
			e.bPressed = bPressed;
			e.button = (MouseButton)button;
			e.mousePosGlobal = mousePosGlobal_;
			e.mousePosLocal = mousePosGlobal_;

			capturingWidgets_.ForEach([&](Widget* w) {
				if(!w) return;
				e.mousePosLocal = GetLocalPosForWidget(w);
				w->OnEvent(&e);
			});
			capturingWidgets_.Clear();
			mousePosOnCaptureGlobal_ = NOPOINT;
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

			for(auto windowIt = widgetStack_.rbegin(); windowIt != widgetStack_.rend(); ++windowIt) {
				auto* window = windowIt->widget.get();
				LogWidgetTree(window);
			}
			return;
		} 
		
		if(button == KeyCode::KEY_D && bPressed) {
			bDrawDebugInfo_ = !bDrawDebugInfo_;
			debugOverlay_->SetVisibility(bDrawDebugInfo_);
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
	}

	void DispatchMouseScrollEvent(float scroll) {}

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
		LOGF(Verbose, "Widget tree: \n{}", ss.str());
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

	std::string PrintAncestorTree(Widget* widget) {
		std::string out;
		PropertyArchive ar;
		for(Widget* w = widget; w; w = w->GetParent()) {
			ar.PushObject(w->GetDebugID(), w, nullptr);
			w->DebugSerialize(ar);
		}
		util::StringBuilder sb(&out);
		PrintPropertyArchive(ar, &sb);
		return out;
	}

	void PrintPropertyArchive(const PropertyArchive& ar, util::StringBuilder* sb) {
		for(auto it = ar.rootObjects_.rbegin(); it != ar.rootObjects_.rend(); ++it) {
			auto& object = *it;
			sb->Line(object->debugName_);
			sb->PushIndent();

			for(auto& property : object->properties_) {
				sb->Line("{}: {}", property.Name, property.Value);
			}
			sb->PopIndent();
		}
	}

	void PrintHitStack(util::StringBuilder& sb) {
		PropertyArchive ar;

		for(auto& hitData : lastHitStack_) {
			if(!hitData.widget) continue;
			ar.PushObject(hitData.widget->GetDebugID(), *hitData.widget, nullptr);
			hitData.widget->DebugSerialize(ar);
		}

		for(auto it = ar.rootObjects_.rbegin(); it != ar.rootObjects_.rend(); ++it) {
			auto& object = *it;
			sb.Line(object->debugName_);
			sb.PushIndent();

			for(auto& property : object->properties_) {
				sb.Line("{}: {}", property.Name, property.Value);
			}
			sb.PopIndent();
		}
	}

	// Update cached widgets for mouse capture and hovering
	void ResetState() {
		hoveredWindow_ = nullptr;
		hoveredWidgets_.Clear();
		capturingWidgets_.Clear();
		bResetState_ = true;
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
			widget->OnEvent(&drawEvent);

			if(bDrawDebugClipRects_) {
				canvas.DrawClipRect(false, colors::red);
			}
			if(auto* layout = widget->As<LayoutWidget>()) {
				if(bDrawDebugLayout_){
					if(lastHitStack_.TopWidget() == layout) {
						canvas.DrawRect(layout->GetRect().Expand(-1), colors::green.MultiplyAlpha(0.3f), true);
					} else {
						canvas.DrawRect(layout->GetRect().Expand(-1), colors::green, false);
					}
				}
				canvas.PushTransform(layout->GetTransform());
				canvas.SaveContext();
				widget->VisitChildren(drawFunc, false);
				canvas.RestoreContext();
			} else {
				widget->VisitChildren(drawFunc, false);
			}
			return visitResultContinue;
		};
		drawFunc(window);
	}

	void UpdateLayout(Widget* widget) {
		LayoutWidget* layoutWidget = nullptr;

		for(Widget* ancestor = widget->GetParent(); ancestor; ancestor = ancestor->GetParent()) {
			if(auto* l = ancestor->As<LayoutWidget>()) {
				const auto axisMode = l->GetAxisMode();
				const auto isLayoutDependsOnChildren =
					axisMode.x == AxisMode::Shrink || axisMode.y == AxisMode::Shrink;

				if(isLayoutDependsOnChildren) {
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
		const bool isRoot = layoutWidget->FindParentOfClass<LayoutWidget>() == nullptr;

		if(isRoot) {
			e.rect = Rect(float2(0), g_OSWindow->GetSize());
		} else {
			const auto layoutInfo = *layoutWidget->GetLayoutStyle();
			e.rect = Rect(
                layoutWidget->GetOrigin() - layoutInfo.margins.Size(),
                layoutWidget->GetSize() + layoutInfo.margins.Size()
			);
		}
		layoutWidget->OnLayout(e);
		layoutWidget->OnPostLayout();
	}

	void BuildDirty() {
		std::function<VisitResult(StatefulWidget*)> build;
		// TODO: Add "BuildStackTrace" for build debugging
		// - Widget being requested
		// - Index of the child being iterated

		// Check if dirty
		// #1 Call build()
		// Mount the new tree and keep the old one
		// Find in the new tree stateful widgets
		// 		Get the old StatefulWidget from the state of the new one
		// 		Update the old widget with the new one
		//		Check if it needs rebuils after update
		//		If do: goto #1
		// 		Else: parent the old tree to the new one
		//	Update layout
		build = [&](StatefulWidget* widget) {
			auto* state = widget->GetState();

			if(!state) {
				auto s = PrintAncestorTree(widget);
				LOGF(Error, "Stafeful widget without state found during Build. Ancestor tree:\n{}", s);
				Assert(false);
			}

			if(widget->NeedsRebuild()) {
				std::unique_ptr<Widget> oldChild = widget->GetChild() ? widget->OrphanChild() : nullptr;

				if(state->IsVisible()) {
					widget->Parent(widget->Build());	
					auto* newChild = widget->GetChild();
					// LOGF(Verbose, "Rebuild the widget {}, old child {}, new child {}", 
					// 	widget->GetDebugID(), 
					// 	oldChild ? oldChild->GetDebugID() : "", 
					// 	newChild->GetDebugID());
					
					widget->VisitChildrenRecursively([&build](Widget* child) {
						if(auto* w = child->As<StatefulWidget>()) {
							return build(w);
						}
						return visitResultContinue;
					});
				}
			} else {
				auto* state = widget->GetState();
				auto* oldWidget = state->GetWidget();

				if(oldWidget) {
					std::unique_ptr<Widget> oldChild = oldWidget->OrphanChild();
					widget->Parent(std::move(oldChild));
				}
			}
			widget->RebindToState();
			return visitResultSkipChildren;
		};

		for(auto& widget: dirtyWidgets_) {
			if(widget) {
				build(widget.Get());
			}
		}
	}

public:

	bool Tick() final {
		OPTICK_FRAME("UI_Tick");
		const auto frameStartTimePoint = std::chrono::high_resolution_clock::now();

		// Rebuild fonts if needed for different size
		{
			OPTICK_EVENT("Rebuilding fonts");
			RebuildFonts();
		}

		if(bResetState_) {
			DispatchMouseMoveEvent(mousePosGlobal_);
			bResetState_ = false;
		}

		{
			OPTICK_EVENT("Polling events");
			if(!g_OSWindow->PollEvents()) {
				return false;
			}
			timers_.Tick();
		}

		// Rebuild
		{
			if(!dirtyWidgets_.empty()) {
				BuildDirty();	
			}
		}

		// Update layout
		{
			for(auto& widget: dirtyWidgets_) {
				if(widget) {
					UpdateLayout(widget.Get());
				}
			}
			dirtyWidgets_.clear();
		}

		if(bDrawDebugInfo_) {
			std::string str;
			auto sb = util::StringBuilder(&str);

			sb.Line("Frame time: {:5.2f}ms", lastFrameTimeMs_);
			sb.Line("Opened window count: {}", widgetStack_.size());
			sb.Line("Timers count: {}", timers_.Size());
			sb.Line("Hovered window: {}", hoveredWindow_ ? hoveredWindow_->GetDebugID() : "");
			sb.Line("Hovered widget: {}", !hoveredWidgets_.Empty() ? hoveredWidgets_.Print() : "");
			sb.Line("Mouse pos local: {}", hoveredWidgets_.Top() ? GetLocalPosForWidget(hoveredWidgets_.Top()) : mousePosGlobal_);
			sb.Line("Mouse pos global: {}", mousePosGlobal_ == NOPOINT ? "npos" : std::format("{}", mousePosGlobal_));
			sb.Line("Capturing mouse widgets: {}", !capturingWidgets_.Empty() ? capturingWidgets_.Print() : "");
			sb.Line();

			if(hoveredWindow_) {
				sb.Line("{} Hit stack: ", hoveredWindow_->GetDebugID());
				sb.PushIndent();
				PrintHitStack(sb);
			}
			debugOverlay_->SetText(str);
		}

		// Draw views
		g_Renderer->ResetDrawLists();
		auto* frameDrawList = g_Renderer->GetFrameDrawList();
		frameDrawList->PushFont(theme_->GetDefaultFont(), g_DefaultFontSize);
		{
			OPTICK_EVENT("Drawing UI");

			// DrawlistImpl canvas;
			// canvas.RendererDrawList = frameDrawList;
			// DrawEvent drawEvent;
			// drawEvent.canvas = &canvas;
			// drawEvent.theme = theme_.get();

			for(auto& [window, layer] : widgetStack_) {
				DrawWindow(window.get(), frameDrawList, theme_.get());
			}
		}
		const auto frameEndTimePoint = std::chrono::high_resolution_clock::now();

		++frameNum_;
		lastFrameTimeMs_ = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTimePoint - frameStartTimePoint).count() / 1000.f;

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
		// Ancestors of the widget should be scheduled after
		// for(auto it = dirtyWidgets_.begin(); it != dirtyWidgets_.end(); ++it) {
		// 	auto& w = *it;
		// 	for(auto* parent = widget->GetParent(); parent; parent = parent->GetParent()) {
		// 		if(parent == w) {
		// 			dirtyWidgets_.insert(it, widget);
		// 			return;
		// 		}
		// 	}
		// }
		dirtyWidgets_.push_back(widget->GetWeak());
	}

	void BringToFront(Widget* widget) override {
		using Iterator = decltype(widgetStack_)::iterator;
		auto found = false;
		// Bring the child just below the overlay layer		
		for(auto it = widgetStack_.begin(); it != widgetStack_.end(); ++it) {
			if(!found && it->widget.get() == widget) {
				found = true;
			} 
			if (found) {
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

	bool OnEvent(IEvent* e) {
		return false;
	}

	void Shutdown() final {}

	Theme* GetTheme() final {
		Assertm(theme_, "A theme should be set before creating widgets");
		return theme_.get();
	}

	void SetTheme(Theme* theme) final {
		// If already has a theme, merge two themes overriding existing
		if(theme_) {
			theme_->Merge(theme);
		} else {
			theme_.reset(theme);
		}
		// Add our default font size
		theme_->GetDefaultFont()->RasterizeFace(g_DefaultFontSize);
		RebuildFonts();
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

	// Handle to the rendering job completion event
	// We will wait for it before kicking new rendering job
	JobSystem::EventRef		renderingJobEventRef_;

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
	// WeakPtr<Widget>			hoveredWidget_;
	WidgetList				hoveredWidgets_;
	WidgetList				capturingWidgets_;
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

UI::Application* UI::Application::Create(std::string_view windowTitle, u32 width, u32 height) {
	if(!g_OSWindow) { g_OSWindow = INativeWindow::createWindow(windowTitle, width, height); }
	if(!g_Application) g_Application = new ApplicationImpl();
	g_Application->Init();
	return g_Application;
}

UI::Application* UI::Application::Get() {
	return g_Application;
}

UI::FrameState UI::Application::GetState() {
	return g_Application->GetFrameStateImpl();
}

GAVAUI_END