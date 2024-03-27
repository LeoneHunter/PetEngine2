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
		WeakPtr<Widget>	Widget;
		TimerCallback	Callback;
		u64				PeriodMs;
		TimePoint		TimePoint;
	};

public:

	TimerHandle AddTimer(Widget* inWidget, const TimerCallback& inCallback, u64 inPeriodMs) {
		m_Timers.emplace_back(Timer(inWidget->GetWeakAs<Widget>(), inCallback, inPeriodMs, Now()));
		return (TimerHandle)&m_Timers.back();
	}

	void		RemoveTimer(TimerHandle inHandle) {
		for(auto it = m_Timers.begin(); it != m_Timers.end(); ++it) {
			if(&*it == inHandle) {
				m_Timers.erase(it);
				break;
			}
		}
	}

	// Ticks timers and calls callbacks
	void		Tick() {
		if(m_Timers.empty()) return;

		const auto now = Now();
		std::vector<std::list<Timer>::const_iterator> pendingDelete;

		for(auto it = m_Timers.begin(); it != m_Timers.end(); ++it) {
			auto& timer = *it;

			if(!timer.Widget) {
				pendingDelete.push_back(it);
				continue;
			}

			if(DurationMs(timer.TimePoint, now) >= timer.PeriodMs) {
				const auto bContinue = timer.Callback();

				if(bContinue) {
					timer.TimePoint = now;
				} else {
					pendingDelete.push_back(it);
				}
			}
		}

		for(auto& it : pendingDelete) {
			m_Timers.erase(it);
		}
	}

	size_t		Size() const { return m_Timers.size(); }

private:

	constexpr u64 DurationMs(const TimePoint& p0, const TimePoint& p1) {
		return p1 >= p0
			? std::chrono::duration_cast<std::chrono::milliseconds>(p1 - p0).count()
			: std::chrono::duration_cast<std::chrono::milliseconds>(p0 - p1).count();
	}

	TimePoint Now() { return std::chrono::high_resolution_clock::now(); }

private:
	std::list<Timer> m_Timers;
};



/*
* Helper to draw text vertically in a sinle column
* for debuging
*/
class VerticalTextDrawer {

	constexpr static inline auto cursorOffset = float2(0.f, 15.f);
	constexpr static inline auto indent = 15.f;
public:

	VerticalTextDrawer(DrawList* inDrawList, Point inCursor, Color inColor)
		: m_DrawList(inDrawList)
		, m_Cursor(inCursor)
		, m_Color(inColor)
		, m_Indent(0) {}

	void DrawText(const std::string& inText) {
		m_DrawList->DrawText(m_Cursor + float2(m_Indent, 0.f), m_Color, inText);
		m_Cursor += cursorOffset;
	}

	void DrawText(std::string_view inText) {
		m_DrawList->DrawText(m_Cursor + float2(m_Indent, 0.f), m_Color, inText.data());
		m_Cursor += cursorOffset;
	}

	void DrawText(const char* inText) {
		m_DrawList->DrawText(m_Cursor + float2(m_Indent, 0.f), m_Color, inText);
		m_Cursor += cursorOffset;
	}

	template<typename ...ArgTypes>
	void DrawTextF(const std::format_string<ArgTypes...> inFormat, ArgTypes... inArgs) {
		m_DrawList->DrawText(m_Cursor + float2(m_Indent, 0.f), m_Color, std::format(inFormat, std::forward<ArgTypes>(inArgs)...));
		m_Cursor += cursorOffset;
	}

	void PushIndent() { m_Indent += indent; }
	void PopIndent() { m_Indent -= indent; }

private:
	Point		m_Cursor;
	DrawList*   m_DrawList;
	Color		m_Color;
	float		m_Indent;
};



class CanvasImpl final: public Canvas {
public:
	CanvasImpl(DrawList* inDrawList)
		: drawList(inDrawList) {}

	void DrawBox(Rect inRect, const BoxStyle* inStyle) override {
		if(inStyle->opacity == 0.f) return;
		inRect = Transform(inRect);
		Rect backgroundRect;
		Rect borderRect;

		if(inStyle->borderAsOutline) {
			backgroundRect = inRect;
			borderRect = Rect(backgroundRect).Expand(inStyle->borders);
		} else {
			borderRect = inRect;
			backgroundRect = Rect(inRect).Expand(
				(float)-inStyle->borders.Top,
				(float)-inStyle->borders.Right,
				(float)-inStyle->borders.Bottom,
				(float)-inStyle->borders.Left);
		}

		auto borderColor = ColorFloat4(inStyle->borderColor);
		auto backgroundColor = ColorFloat4(inStyle->backgroundColor);

		borderColor.a *= inStyle->opacity;
		backgroundColor.a *= inStyle->opacity;

		if(inStyle->borders.Left || inStyle->borders.Right || inStyle->borders.Top || inStyle->borders.Bottom) {

			if(backgroundColor.a != 1.f) {
				drawList->DrawRect(borderRect, ColorU32(borderColor), inStyle->rounding, (DrawList::Corner)inStyle->roundingMask);
			} else {
				drawList->DrawRectFilled(borderRect, ColorU32(borderColor), inStyle->rounding, (DrawList::Corner)inStyle->roundingMask);
			}
		}

		drawList->DrawRectFilled(backgroundRect, ColorU32(backgroundColor), inStyle->rounding, (DrawList::Corner)inStyle->roundingMask);
	}

	void DrawRect(Rect inRect, Color inColor, bool bFilled = true) override {
		inRect = Transform(inRect);
		if(bFilled) {
			drawList->DrawRectFilled(inRect, ColorU32(inColor));
		} else {
			drawList->DrawRect(inRect, ColorU32(inColor));
		}
	}

	void DrawText(Point inOrigin, const TextStyle* inStyle, std::string_view inTextView) override {
		inOrigin = Transform(inOrigin);
		drawList->DrawText(inOrigin, inStyle->color, inTextView, inStyle->fontSize, inStyle->fontWeightBold, inStyle->fontStyleItalic);
	}

	void ClipRect(Rect inClipRect) override {
		if(!context.clipRect.Empty()) {
			drawList->PopClipRect();
		}	
		inClipRect = Transform(inClipRect);
		context.clipRect = inClipRect;
		drawList->PushClipRect(inClipRect);
	}

	void DrawClipRect(bool bFilled, Color inColor) {
		if(!context.clipRect.Empty()) {
			drawList->PopClipRect();

			if(bFilled) {
				drawList->DrawRectFilled(context.clipRect, inColor);
			} else {
				drawList->DrawRect(context.clipRect, inColor);
			}
			drawList->PushClipRect(context.clipRect);
		}
	}

	void PushTransform(float2 inTransform) {
		context.transform = inTransform;
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

	Point Transform(Point inPoint) {
		return inPoint + cummulativeTransform + context.transform;
	}

	Rect Transform(Rect inRect) {
		return Rect(inRect).Translate(cummulativeTransform + context.transform);
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

	void Push(Widget* inItem) {
		stack.push_back(inItem->GetWeak());
	}

	void ForEach(const std::function<void(Widget*)>& fn) {
		for(auto& item: stack) {
			if(!item) continue;
			fn(item.Get());
		}
	}

	constexpr void Remove(Widget* inItem) {
		for(auto it = stack.begin(); it != stack.end(); ++it) {
			if(*it == inItem) {
				stack.erase(it);
				break;
			}
		}
	}

	constexpr bool Contains(Widget* inItem) {
		for(auto it = stack.begin(); it != stack.end(); ++it) {
			if(*it == inItem) {
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
			.Child(TextBox::New(text))
			.New();
	}

	void SetText(const std::string& inText) {
		text = inText;
		MarkNeedsRebuild();
	}

	std::string text;
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

		m_Theme.reset(new Theme());

		auto& debugOverlayStyle = m_Theme->Add(names::debugOverlayWindowID);
		debugOverlayStyle.Add<LayoutStyle>().Margins(5, 5).Paddings(5, 5);
		debugOverlayStyle.Add<BoxStyle>().FillColor("#454545dd").Rounding(4);

		RebuildFonts();
		m_DebugOverlay = std::make_unique<DebugOverlayWindow>();
		Parent(StatefulWidget::New(m_DebugOverlay.get()), Layer::Overlay);

		g_OSWindow->SetOnCursorMoveCallback([](float x, float y) { g_Application->DispatchMouseMoveEvent({x, y}); });
		g_OSWindow->SetOnMouseButtonCallback([](KeyCode inButton, bool bPressed) { g_Application->DispatchMouseButtonEvent(inButton, bPressed); });
		g_OSWindow->SetOnMouseScrollCallback([](float inScroll) { g_Application->DispatchMouseScrollEvent(inScroll); });
		g_OSWindow->SetOnWindowResizedCallback([](float2 inWindowSize) { g_Application->DispatchOSWindowResizeEvent(inWindowSize); });
		g_OSWindow->SetOnKeyboardButtonCallback([](KeyCode inButton, bool bPressed) { g_Application->DispatchKeyEvent(inButton, bPressed); });
		g_OSWindow->SetOnCharInputCallback([](wchar_t inCharacter) { g_Application->DispatchCharInputEvent(inCharacter); });

		DispatchOSWindowResizeEvent(g_OSWindow->GetSize());
	}

	void DispatchMouseMoveEvent(Point inMousePosGlobal) {
		const auto mousePosGlobal = inMousePosGlobal;
		const auto mouseDelta = mousePosGlobal - m_MousePosGlobal;
		m_MousePosGlobal = mousePosGlobal;

		MouseDragEvent mouseMoveEvent;
		mouseMoveEvent.mousePosOnCaptureLocal = m_MousePosOnCaptureGlobal;
		mouseMoveEvent.mousePosLocal = mousePosGlobal;
		mouseMoveEvent.mouseDelta = mouseDelta;
		mouseMoveEvent.mouseButtonsPressedBitField = m_MouseButtonsPressedBitField;

		if(mousePosGlobal == NOPOINT) {
			auto e = HoverEvent::LeaveEvent();
			m_HoveredWidgets.ForEach([&](Widget* inWidget) {
				inWidget->OnEvent(&e);
			});
			return;
		} 
		
		if(!m_CapturingWidgets.Empty()) {
			m_CapturingWidgets.ForEach([&](Widget* w) {
				const auto mouseDeltaFromInitial = mousePosGlobal - m_MousePosOnCaptureGlobal;
				// Convert global to local using hit test data
				const auto* parentHitData = m_LastHitStack.Find(w->FindParentOfClass<LayoutWidget>());
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
		m_HoveredWindow = nullptr;
		bool bHasPopups = false;

		for(auto it = m_WidgetStack.rbegin(); it != m_WidgetStack.rend(); ++it) {
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
				m_HoveredWindow = hoveredWindow->GetWeak();		
				break;
			}
		}
		WidgetList prevHovered = m_HoveredWidgets;
		WidgetList newHovered;

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

					if(bPrevHovered)
						prevHovered.Remove(widget);

					if(!bHandled && !bAlways)
						bHandled = true;
				}
			}
		}
		// Dispatch leave events to the widgets left in the prevHovered list
		// These items have not been transfered to the new list wich means they're no longer hovered
		auto e = HoverEvent::LeaveEvent();

		prevHovered.ForEach([&](Widget* inWidget) {
			inWidget->OnEvent(&e);
		});
		
		m_HoveredWidgets = newHovered;
		m_LastHitStack = hitStack;
	}

	Point GetLocalPosForWidget(Widget* inWidget) {
		auto out = m_MousePosGlobal;
		
		if(auto* layoutParent = inWidget->FindParentOfClass<LayoutWidget>()) {
			if(auto* hitData = m_LastHitStack.Find(layoutParent)) {
				out = hitData->hitPosLocal;
			}
		}
		return out;
	}

	void DispatchMouseButtonEvent(KeyCode inButton, bool bPressed) {

		bPressed ? ++m_MouseButtonHeldNum : --m_MouseButtonHeldNum;
		bPressed ? m_MouseButtonsPressedBitField |= (MouseButtonMask)inButton
			: m_MouseButtonsPressedBitField &= ~(MouseButtonMask)inButton;

  		// When pressed first time from default
		if(bPressed && m_MouseButtonHeldNum == 1) {
			m_PressedMouseButton = (MouseButton)inButton;

			MouseButtonEvent event;
			event.mousePosGlobal = m_MousePosGlobal;
			event.mousePosLocal = m_MousePosGlobal;
			event.bPressed = bPressed;
			event.button = (MouseButton)inButton;
			bool bHandled = false;

			for(auto it = m_LastHitStack.begin(); it != m_LastHitStack.end(); ++it) {
				auto& [layoutWidget, pos] = *it;
				// In case widget has been deleted between events
				if(!layoutWidget) continue;
				event.mousePosLocal = m_MousePosGlobal;

				if(auto parentIt = it + 1; parentIt != m_LastHitStack.end()) {
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
							m_CapturingWidgets.Push(widget.Get());
							
							if(!bHandled && !bAlways) {
								bHandled = true;
								m_MousePosOnCaptureGlobal = m_MousePosGlobal;
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

		if(!m_CapturingWidgets.Empty() && !bPressed && (MouseButton)inButton == m_PressedMouseButton) {
			MouseButtonEvent e;
			e.bPressed = bPressed;
			e.button = (MouseButton)inButton;
			e.mousePosGlobal = m_MousePosGlobal;
			e.mousePosLocal = m_MousePosGlobal;

			m_CapturingWidgets.ForEach([&](Widget* w) {
				if(!w) return;
				e.mousePosLocal = GetLocalPosForWidget(w);
				w->OnEvent(&e);
			});
			m_CapturingWidgets.Clear();
			m_MousePosOnCaptureGlobal = NOPOINT;
			m_PressedMouseButton = MouseButton::None;
		}
	}

	void	DispatchKeyEvent(KeyCode inButton, bool bPressed) {

		if(inButton == KeyCode::KEY_LEFT_SHIFT) m_ModifiersState[LeftShift] = !m_ModifiersState[LeftShift];
		if(inButton == KeyCode::KEY_RIGHT_SHIFT) m_ModifiersState[RightShift] = !m_ModifiersState[RightShift];

		if(inButton == KeyCode::KEY_LEFT_CONTROL) m_ModifiersState[LeftControl] = !m_ModifiersState[LeftControl];
		if(inButton == KeyCode::KEY_RIGHT_CONTROL) m_ModifiersState[RightControl] = !m_ModifiersState[RightControl];

		if(inButton == KeyCode::KEY_LEFT_ALT) m_ModifiersState[LeftAlt] = !m_ModifiersState[LeftAlt];
		if(inButton == KeyCode::KEY_RIGHT_ALT) m_ModifiersState[RightAlt] = !m_ModifiersState[RightAlt];

		if(inButton == KeyCode::KEY_CAPS_LOCK) m_ModifiersState[CapsLock] = !m_ModifiersState[CapsLock];

		if(inButton == KeyCode::KEY_P && bPressed) {

			for(auto windowIt = m_WidgetStack.rbegin(); windowIt != m_WidgetStack.rend(); ++windowIt) {
				auto* window = windowIt->widget.get();
				LogWidgetTree(window);
			}
			return;
		} 
		
		if(inButton == KeyCode::KEY_D && bPressed) {
			m_bDrawDebugInfo = !m_bDrawDebugInfo;
			m_DebugOverlay->SetVisibility(m_bDrawDebugInfo);
			return;
		}

		if(inButton == KeyCode::KEY_L && bPressed) {
			m_bDrawDebugLayout = !m_bDrawDebugLayout;
			return;
		}
		
		if(inButton == KeyCode::KEY_C && bPressed) {
			m_bDrawDebugClipRects = !m_bDrawDebugClipRects;
			return;
		}
	}

	void	DispatchMouseScrollEvent(float inScroll) {}

	void	DispatchOSWindowResizeEvent(float2 inWindowSize) {
		// Ignore minimized state for now
		if(inWindowSize == float2()) {
			return;
		}
		ParentLayoutEvent layoutEvent;
		layoutEvent.constraints = Rect(inWindowSize);

		for(auto& [window, layer] : m_WidgetStack) {
			UpdateLayout(window.get());
		}
		g_Renderer->ResizeFramebuffers(inWindowSize);
	}

	void	DispatchCharInputEvent(wchar_t inChar) {}

	void	LogWidgetTree(Widget* inWindow) {
		PropertyArchive archive;
		DebugLogEvent onDebugLog;
		onDebugLog.archive = &archive;

		inWindow->OnEvent(&onDebugLog);

		std::deque<PropertyArchive::ObjectID> parentIDStack;
		std::stringstream ss;

		auto printIndent = [&](size_t inIndent) {
			if(!inIndent) return;
			ss << "    ";
			--inIndent;

			for(; inIndent; --inIndent) {
				ss << "|   ";
			}
		};

		auto visitor = [&](PropertyArchive::Object& inObject)->bool {
			const auto parentID = inObject.m_Parent ? inObject.m_Parent->m_ObjectID : 0;

			if(!parentID) {
				parentIDStack.clear();

			} else if(parentID != parentIDStack.back()) {
				// Unwind intil the parent is found
				for(auto stackParentID = parentIDStack.back();
					stackParentID != parentID;
					parentIDStack.pop_back(), stackParentID = parentIDStack.back());
			}
			parentIDStack.push_back(inObject.m_ObjectID);
			auto indent = parentIDStack.size();

			printIndent(indent); ss << '\n';
			printIndent(indent - 1); ss << "|-> " << inObject.m_DebugName << ":\n";

			for(auto& property : inObject.m_Properties) {
				printIndent(indent);
				ss << std::format("{}: {}", property.Name, property.Value) << '\n';
			}
			return true;
		};
		archive.VisitRecursively(visitor);
		LOGF(Verbose, "Widget tree: \n{}", ss.str());
	}

	// Rebuild font of the theme if needed
	void	RebuildFonts() {
		std::vector<Font*> fonts;
		m_Theme->RasterizeFonts(&fonts);

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

	std::string PrintAncestorTree(Widget* inWidget) {
		std::string out;
		PropertyArchive ar;
		for(Widget* w = inWidget; w; w = w->GetParent()) {
			ar.PushObject(w->GetDebugID(), w, nullptr);
			w->DebugSerialize(ar);
		}
		util::StringBuilder sb(&out);
		PrintPropertyArchive(ar, &sb);
		return out;
	}

	void PrintPropertyArchive(const PropertyArchive& ar, util::StringBuilder* sb) {
		for(auto it = ar.m_RootObjects.rbegin(); it != ar.m_RootObjects.rend(); ++it) {
			auto& object = *it;
			sb->Line(object->m_DebugName);
			sb->PushIndent();

			for(auto& property : object->m_Properties) {
				sb->Line("{}: {}", property.Name, property.Value);
			}
			sb->PopIndent();
		}
	}

	void	PrintHitStack(util::StringBuilder& sb) {
		PropertyArchive ar;

		for(auto& hitData : m_LastHitStack) {
			if(!hitData.widget) continue;
			ar.PushObject(hitData.widget->GetDebugID(), *hitData.widget, nullptr);
			hitData.widget->DebugSerialize(ar);
		}

		for(auto it = ar.m_RootObjects.rbegin(); it != ar.m_RootObjects.rend(); ++it) {
			auto& object = *it;
			sb.Line(object->m_DebugName);
			sb.PushIndent();

			for(auto& property : object->m_Properties) {
				sb.Line("{}: {}", property.Name, property.Value);
			}
			sb.PopIndent();
		}
	}

	// Update cached widgets for mouse capture and hovering
	void	ResetState() {
		m_HoveredWindow = nullptr;
		m_HoveredWidgets.Clear();
		m_CapturingWidgets.Clear();
		m_bResetState = true;
	}
	
	// We will iterate a subtree manually and handle nesting and visibility
	// Possibly opens a possibility to cache draw commands
	// Also we will handle clip rects nesting
	void	DrawWindow(Widget* inWindow, DrawList* inRendererDrawlist, Theme* inTheme) {
		CanvasImpl canvas(inRendererDrawlist);

		DrawEvent drawEvent;
		drawEvent.canvas = &canvas;
		drawEvent.theme = inTheme;

		std::function<VisitResult(Widget*)> drawFunc;
		// Draw children recursively using DFS
		drawFunc = [&](Widget* inWidget) {
			inWidget->OnEvent(&drawEvent);

			if(m_bDrawDebugClipRects) {
				canvas.DrawClipRect(false, colors::red);
			}
			if(auto* layout = inWidget->As<LayoutWidget>()) {
				if(m_bDrawDebugLayout){
					if(m_LastHitStack.TopWidget() == layout) {
						canvas.DrawRect(layout->GetRect().Expand(-1), colors::green.MultiplyAlpha(0.3f), true);
					} else {
						canvas.DrawRect(layout->GetRect().Expand(-1), colors::green, false);
					}
				}
				canvas.PushTransform(layout->GetTransform());
				canvas.SaveContext();
				inWidget->VisitChildren(drawFunc, false);
				canvas.RestoreContext();
			} else {
				inWidget->VisitChildren(drawFunc, false);
			}
			return visitResultContinue;
		};
		drawFunc(inWindow);
	}

	void UpdateLayout(Widget* inWidget) {
		LayoutWidget* layoutWidget = nullptr;

		for(Widget* w = inWidget->GetParent(); w; w = w->GetParent()) {
			if(auto* l = w->As<LayoutWidget>()) {
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
			layoutWidget = LayoutWidget::FindNearest(inWidget);
		}
		if(!layoutWidget) {
			return;
		}
		// Update layout of children recursively starting from this widget
		ParentLayoutEvent e;
		const bool isRoot = layoutWidget->FindParentOfClass<LayoutWidget>() == nullptr;

		if(isRoot) {
			e.constraints = Rect(float2(0), g_OSWindow->GetSize());
		} else {
			const auto layoutInfo = *layoutWidget->GetLayoutStyle();
			e.constraints = Rect(
                layoutWidget->GetOrigin() - layoutInfo.margins.Size(),
                layoutWidget->GetSize() + layoutInfo.margins.Size()
			);
		}
		layoutWidget->OnLayout(e);
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

		for(auto& widget: m_DirtyWidgets) {
			if(widget) {
				build(widget.Get());
			}
		}
	}

public:

	bool	Tick() final {
		OPTICK_FRAME("UI_Tick");
		const auto frameStartTimePoint = std::chrono::high_resolution_clock::now();

		// Rebuild fonts if needed for different size
		{
			OPTICK_EVENT("Rebuilding fonts");
			RebuildFonts();
		}

		if(m_bResetState) {
			DispatchMouseMoveEvent(m_MousePosGlobal);
			m_bResetState = false;
		}

		{
			OPTICK_EVENT("Polling events");
			if(!g_OSWindow->PollEvents()) {
				return false;
			}
			m_Timers.Tick();
		}

		// Rebuild
		{
			if(!m_DirtyWidgets.empty()) {
				BuildDirty();	
			}
		}

		// Update layout
		{
			for(auto& widget: m_DirtyWidgets) {
				if(widget) {
					UpdateLayout(widget.Get());
				}
			}
			m_DirtyWidgets.clear();
		}

		if(m_bDrawDebugInfo) {
			std::string str;
			auto sb = util::StringBuilder(&str);

			sb.Line("Frame time: {:5.2f}ms", m_LastFrameTimeMs);
			sb.Line("Opened window count: {}", m_WidgetStack.size());
			sb.Line("Timers count: {}", m_Timers.Size());
			sb.Line("Hovered window: {}", m_HoveredWindow ? m_HoveredWindow->GetDebugID() : "");
			sb.Line("Hovered widget: {}", !m_HoveredWidgets.Empty() ? m_HoveredWidgets.Print() : "");
			sb.Line("Mouse pos local: {}", m_HoveredWidgets.Top() ? GetLocalPosForWidget(m_HoveredWidgets.Top()) : m_MousePosGlobal);
			sb.Line("Mouse pos global: {}", m_MousePosGlobal == NOPOINT ? "npos" : std::format("{}", m_MousePosGlobal));
			sb.Line("Capturing mouse widgets: {}", !m_CapturingWidgets.Empty() ? m_CapturingWidgets.Print() : "");
			sb.Line();

			if(m_HoveredWindow) {
				sb.Line("{} Hit stack: ", m_HoveredWindow->GetDebugID());
				sb.PushIndent();
				PrintHitStack(sb);
			}
			m_DebugOverlay->SetText(str);
		}

		// Draw views
		g_Renderer->ResetDrawLists();
		auto* frameDrawList = g_Renderer->GetFrameDrawList();
		frameDrawList->PushFont(m_Theme->GetDefaultFont(), g_DefaultFontSize);
		{
			OPTICK_EVENT("Drawing UI");

			// DrawlistImpl canvas;
			// canvas.RendererDrawList = frameDrawList;
			// DrawEvent drawEvent;
			// drawEvent.canvas = &canvas;
			// drawEvent.theme = m_Theme.get();

			for(auto& [window, layer] : m_WidgetStack) {
				DrawWindow(window.get(), frameDrawList, m_Theme.get());
			}
		}
		const auto frameEndTimePoint = std::chrono::high_resolution_clock::now();

		++m_FrameNum;
		m_LastFrameTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTimePoint - frameStartTimePoint).count() / 1000.f;

		g_Renderer->RenderFrame(true);
		return true;
	}
	
	std::unique_ptr<Widget> Orphan(Widget* inWidget) override {
		// Remove child without deletion
		for(auto it = m_WidgetStack.begin(); it != m_WidgetStack.end(); ++it) {
			if(it->widget.get() == inWidget) {
				auto out = std::move(it->widget);
				m_WidgetStack.erase(it);
				return out;
			}
		}
		return {};
	}
	
	void RequestRebuild(StatefulWidget* inWidget) override {
		// Ancestors of the widget should be scheduled after
		// for(auto it = m_DirtyWidgets.begin(); it != m_DirtyWidgets.end(); ++it) {
		// 	auto& w = *it;
		// 	for(auto* parent = inWidget->GetParent(); parent; parent = parent->GetParent()) {
		// 		if(parent == w) {
		// 			m_DirtyWidgets.insert(it, inWidget);
		// 			return;
		// 		}
		// 	}
		// }
		m_DirtyWidgets.push_back(inWidget->GetWeak());
	}

	void BringToFront(Widget* inWidget) override {
		using Iterator = decltype(m_WidgetStack)::iterator;
		auto found = false;
		// Bring the child just below the overlay layer		
		for(auto it = m_WidgetStack.begin(); it != m_WidgetStack.end(); ++it) {
			if(!found && it->widget.get() == inWidget) {
				found = true;
			} 
			if (found) {
				auto nextItem = ++Iterator(it);
				if(nextItem != m_WidgetStack.end() && nextItem->layer != Layer::Overlay) {
					std::swap(it->widget, nextItem->widget);
				}
			}			
		}
	}

	void Parent(std::unique_ptr<Widget>&& inWidget, Layer inLayer) override {
		auto* widget = inWidget.get();
		// Insert the widget based on layer
		// Call OnParented()
		if(inLayer == Layer::Overlay) {
			m_WidgetStack.emplace_back(RootWidget(std::move(inWidget), inLayer));

		} else if(inLayer == Layer::Background) {
			m_WidgetStack.emplace_front(RootWidget(std::move(inWidget), inLayer));

		} else {
			for(auto it = m_WidgetStack.begin(); it != m_WidgetStack.end(); ++it) {
				// Insert the window before overlays
				if(it->layer == Layer::Overlay) {
					m_WidgetStack.insert(it, RootWidget(std::move(inWidget), inLayer));
					break;
				}
			}
		}
		widget->OnParented(this);
		auto* statefulChild = widget->As<StatefulWidget>() 
			? widget->As<StatefulWidget>() 
			: widget->FindChildOfClass<StatefulWidget>();

		if(statefulChild) {
			RequestRebuild(statefulChild);	
		} else {
			UpdateLayout(widget);
		}
	}

	bool OnEvent(IEvent* e) {
		return false;
	}

	void	Shutdown() final {}

	Theme* GetTheme() final {
		Assertm(m_Theme, "A theme should be set before creating widgets");
		return m_Theme.get();
	}

	void	SetTheme(Theme* inTheme) final {
		// If already has a theme, merge two themes overriding existing
		if(m_Theme) {
			m_Theme->Merge(inTheme);
		} else {
			m_Theme.reset(inTheme);
		}
		// Add our default font size
		m_Theme->GetDefaultFont()->RasterizeFace(g_DefaultFontSize);
		RebuildFonts();
	}

	FrameState GetFrameStateImpl() {
		auto state = FrameState();
		state.keyModifiersState = m_ModifiersState;
		state.mouseButtonHeldNum = m_MouseButtonHeldNum;
		state.mouseButtonsPressedBitField = m_MouseButtonsPressedBitField;
		state.mousePosGlobal = m_MousePosGlobal;
		state.mousePosOnCaptureGlobal = m_MousePosOnCaptureGlobal;
		state.windowSize = g_OSWindow->GetSize();
		state.theme = m_Theme.get();
		return state;
	}

	TimerHandle	AddTimer(Widget* inWidget, const TimerCallback& inCallback, u64 inPeriodMs) override {
		return m_Timers.AddTimer(inWidget, inCallback, inPeriodMs);
	}

	void RemoveTimer(TimerHandle inHandle) override {
		m_Timers.RemoveTimer(inHandle);
	}

private:

	// Handle to the rendering job completion event
	// We will wait for it before kicking new rendering job
	JobSystem::EventRef		m_RenderingJobEventRef;

	u64						m_FrameNum = 0;
	float					m_LastFrameTimeMs = 0;
	
	// Whether the native window is minimized
	bool					m_bMinimized = false;
	bool					m_bResetState = false;

	// Position of the mouse cursor when button has been pressed
	Point					m_MousePosOnCaptureGlobal;
	Point					m_MousePosGlobal;

	// Number of buttons currently held
	u8						m_MouseButtonHeldNum = 0;
	// We allow only one button to be pressed simultaniously
	MouseButton				m_PressedMouseButton = MouseButton::None;
	MouseButtonMask			m_MouseButtonsPressedBitField = MouseButtonMask::None;
	KeyModifiersArray		m_ModifiersState{false};

	// Layout widgets that are hit by mouse cursor
	HitStack				m_LastHitStack;
	// WeakPtr<Widget>			m_HoveredWidget;
	WidgetList				m_HoveredWidgets;
	WidgetList				m_CapturingWidgets;
	WeakPtr<Widget>			m_HoveredWindow;
	
	// Draw hitstack and mouse pos
	std::unique_ptr<DebugOverlayWindow>	
							m_DebugOverlay;

	bool					m_bDrawDebugInfo = true;
	bool					m_bDrawDebugLayout = false;
	bool					m_bDrawDebugClipRects = false;

	// Stack of windows, bottom are background windows and top are overlay windows
	// Other windows in the middle
	struct RootWidget {
		std::unique_ptr<Widget> widget;
		Layer 					layer;
	};
	std::list<RootWidget>	m_WidgetStack;

	// Widgets which state has been changed and need rebuilding
	// FIXME: It should contain only one widget from a branch
	// Only the topmost ancestor, because when the topmost is rebuild 
	// another widgets will be updated too, and pointers will become stale
	// We will use a weakptr as a workaround
	std::vector<WeakPtr<StatefulWidget>> m_DirtyWidgets;

	TimerList				m_Timers;
	std::unique_ptr<Theme>	m_Theme;

};

UI::Application* UI::Application::Create(std::string_view inWindowTitle, u32 inWidth, u32 inHeight) {
	if(!g_OSWindow) { g_OSWindow = INativeWindow::createWindow(inWindowTitle, inWidth, inHeight); }
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