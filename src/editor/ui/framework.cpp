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
constexpr bool	 g_DrawCliprects = true;

INativeWindow* g_OSWindow = nullptr;
Renderer* g_Renderer = nullptr;
ApplicationImpl* g_Application = nullptr;


namespace names {
	constexpr auto RootBackgroundWindowID = "RootBackgroundWindow";
	constexpr auto DebugOverlayWindowID = "DebugOverlayWindow";
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
	DrawList* m_DrawList;
	Color		m_Color;
	float		m_Indent;
};



class DrawlistImpl final: public Drawlist {
public:

	void PushBox(Rect inRect, const BoxStyle* inStyle) override {
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
				RendererDrawList->DrawRect(borderRect, ColorU32(borderColor), inStyle->rounding, (DrawList::Corner)inStyle->roundingMask);
			} else {
				RendererDrawList->DrawRectFilled(borderRect, ColorU32(borderColor), inStyle->rounding, (DrawList::Corner)inStyle->roundingMask);
			}
		}

		RendererDrawList->DrawRectFilled(backgroundRect, ColorU32(backgroundColor), inStyle->rounding, (DrawList::Corner)inStyle->roundingMask);
	}

	void PushBox(Rect inRect, Color inColor, bool bFilled = true) override {

		if(bFilled) {
			RendererDrawList->DrawRectFilled(inRect, ColorU32(inColor));
		} else {
			RendererDrawList->DrawRect(inRect, ColorU32(inColor));
		}
	}

	void PushText(Point inOrigin, const TextStyle* inStyle, std::string_view inTextView) override {
		RendererDrawList->DrawText(inOrigin, inStyle->color, inTextView, inStyle->fontSize, inStyle->fontWeightBold, inStyle->fontStyleItalic);
	}

	void PushClipRect(Rect inClipRect) override {
		RendererDrawList->PushClipRect(inClipRect);

		if(g_DrawCliprects) {
			RendererDrawList->DrawRect(inClipRect.Expand(-1), colors::red);
		}
	}

	void PopClipRect() override {
		RendererDrawList->PopClipRect();
	}

	void PushTransform(float2 inTransform) override {
		RendererDrawList->PushTransform(inTransform);
	}

	void PopTransform() override {
		RendererDrawList->PopTransform();
	}

	RendererDrawlist* RendererDrawList = nullptr;
};


/*
* Overlay that is drawn over root window
*/
class DebugOverlayWindow: public StatefulWidget {
public:

	DebugOverlayWindow() {
		SetChild(Container::New(
			SizeMode::ShrinkWrap,
			true,
			names::DebugOverlayWindowID,
			Text::New("")));
		FindChildOfClass<LayoutWidget>()->SetOrigin(5, 5);
	}

	void SetText(const std::string& inText) {
		FindChildOfClass<Text>()->SetText(inText);
	}

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

		auto& debugOverlayStyle = m_Theme->Add(names::DebugOverlayWindowID);
		debugOverlayStyle.Add<LayoutStyle>().Margins(5, 5).Paddings(5, 5);
		debugOverlayStyle.Add<BoxStyle>().FillColor("#454545dd").Rounding(4);

		RebuildFonts();

		m_DebugOverlay = new DebugOverlayWindow();
		Parent(m_DebugOverlay, Layer::Overlay);

		g_OSWindow->SetOnCursorMoveCallback([](float x, float y) { g_Application->DispatchMouseMoveEvent({x, y}); });
		g_OSWindow->SetOnMouseButtonCallback([](KeyCode inButton, bool bPressed) { g_Application->DispatchMouseButtonEvent(inButton, bPressed); });
		g_OSWindow->SetOnMouseScrollCallback([](float inScroll) { g_Application->DispatchMouseScrollEvent(inScroll); });
		g_OSWindow->SetOnWindowResizedCallback([](float2 inWindowSize) { g_Application->DispatchOSWindowResizeEvent(inWindowSize); });
		g_OSWindow->SetOnKeyboardButtonCallback([](KeyCode inButton, bool bPressed) { g_Application->DispatchKeyEvent(inButton, bPressed); });
		g_OSWindow->SetOnCharInputCallback([](wchar_t inCharacter) { g_Application->DispatchCharInputEvent(inCharacter); });

		DispatchOSWindowResizeEvent(g_OSWindow->GetSize());
	}

	void DispatchMouseLeaveEvent(Widget* inPivot) {
		if(m_LastHitStack.Empty()) return;
		auto e = HoverEvent::LeaveEvent();

		for(Widget* widget = m_LastHitStack.stack.back().widget.Get();
					widget != inPivot;
					widget = widget->GetParent()) {
			widget->OnEvent(&e);
		}
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
			DispatchMouseLeaveEvent(nullptr);
			return;
		} 
		
		if(m_CapturingWidget) {
			const auto mouseDeltaFromInitial = mousePosGlobal - m_MousePosOnCaptureGlobal;
			// Convert global to local using hit test data
			const auto* parentHitData = m_LastHitStack.Find(m_CapturingWidget->FindParentOfClass<LayoutWidget>());
			const auto  mousePosLocal = parentHitData
				? parentHitData->hitPosLocal + mouseDeltaFromInitial
				: mousePosGlobal;

			MouseDragEvent dragEvent;
			dragEvent.mousePosOnCaptureLocal = mousePosLocal - mouseDeltaFromInitial;
			dragEvent.mousePosLocal = mousePosLocal;
			dragEvent.mouseDelta = mouseDelta;
			m_CapturingWidget->OnEvent(&dragEvent);
			return;
		}

		Widget* 		hoveredWidget = nullptr;
		Widget* 		hoveredWindow = nullptr;
		HitTestEvent	hitTest;
		auto&			hitStack = hitTest.hitStack;
		hitTest.hitPosGlobal = mousePosGlobal;
		m_HoveredWindow = nullptr;

		for(auto it = --m_WidgetStack.end(); it != --m_WidgetStack.begin(); --it) {
			auto& [widget, layer] = *it;

			if(layer == Layer::Overlay) continue;
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
		// The lowest widget in the old tree that's shared between stacks
		Widget* pivot = nullptr;
		auto& oldStack = m_LastHitStack.stack;
		auto& newStack = hitStack.stack;
		
		// Calculate hit stack diff and dispatch MouseLeave and MouseEnter events
		if(!oldStack.empty() && !newStack.empty()) {
			for(auto oldStackIt = oldStack.begin(),
					 newStackIt = newStack.begin();
					 oldStackIt != oldStack.end() &&
					 newStackIt != newStack.end();
					 ++oldStackIt,
					 ++newStackIt) {

				if(newStackIt->widget != oldStackIt->widget) {
					break;
				} else {
					pivot = newStackIt->widget.Get();
				}
			}
		}
		DispatchMouseLeaveEvent(pivot);
		m_LastHitStack = hitStack;
		m_HoveredWidget = nullptr;

		// Dispatch mouse enter events
		if(!newStack.empty()) {
			auto enterEvent = HoverEvent::EnterEvent();

			for(Widget* widget = newStack.back().widget.Get(); 
						widget != pivot; 
						widget = widget->GetParent()) {

				if(widget->OnEvent(&enterEvent) && !enterEvent.bHandled) {
					m_HoveredWidget = widget->GetWeak();
					enterEvent.bHandled = true;
				}
			}
		}

		// Dispatch normal move
		if(pivot) {
			auto e = HoverEvent::Normal();

			for(Widget* widget = pivot; widget; widget = widget->GetParent()) {
				if(widget->OnEvent(&e)) {
					e.bHandled = true;
					if(!m_HoveredWidget) {
						m_HoveredWidget = widget->GetWeak();
					}
				}
			}
		}
	}

	void DispatchMouseButtonEvent(KeyCode inButton, bool bPressed) {

		bPressed ? ++m_MouseButtonHeldNum : --m_MouseButtonHeldNum;
		bPressed ? m_MouseButtonsPressedBitField |= (MouseButtonMask)inButton
			: m_MouseButtonsPressedBitField &= ~(MouseButtonMask)inButton;

		auto Propagate = [this](Widget* startWidget) {
		};

  		 // When pressed first time from default
		if(bPressed && m_MouseButtonHeldNum == 1) {
			m_PressedMouseButton = (MouseButton)inButton;

			if(m_HoveredWidget) {
				MouseButtonEvent event;
				event.mousePosGlobal = m_MousePosGlobal;
				event.mousePosLocal = m_MousePosGlobal;
				event.bButtonPressed = bPressed;
				event.button = (MouseButton)inButton;

				for(auto it = m_LastHitStack.begin(); it != m_LastHitStack.end(); ++it) {
					auto& [layoutWidget, pos] = *it;
					// In case widget has been deleted between events
					if(!layoutWidget) continue;
					event.mousePosLocal = m_MousePosGlobal;

					if(auto parentIt = it + 1; parentIt != m_LastHitStack.end()) {
						event.mousePosLocal = parentIt->hitPosLocal;
					}
					// Move up until another LayoutWidget is found
					// So any widget placed between LayoutWidgets has a chance to handle this event
					// TODO we should be careful not to delete the widget that handles the event on button press
					Widget* widget = layoutWidget.Get();

					do {
						LOGF("Button event {} dispatched to {}", event.GetDebugID(), widget->GetDebugID());
						LOGF("Local mouse pos: {}", event.mousePosLocal);
						const auto bHandled = widget->OnEvent(&event);
						// A widget could be deleted in the OnEvent call
						if(!event.bHandled && bHandled && layoutWidget) {
							LOGF("Event handled by {}", widget->GetDebugID());
							event.bHandled = true;
							m_CapturingWidget = widget->GetWeak();
							m_MousePosOnCaptureGlobal = m_MousePosGlobal;
						}
						widget = widget->GetParent();
					} while(widget && !widget->IsA<LayoutWidget>());
				}
			}
			return;
		}

		if(m_CapturingWidget && !bPressed && (MouseButton)inButton == m_PressedMouseButton) {
			MouseButtonEvent mouseButtonEvent;
			mouseButtonEvent.bButtonPressed = bPressed;
			mouseButtonEvent.button = (MouseButton)inButton;
			mouseButtonEvent.mousePosGlobal = m_MousePosGlobal;

			for(Widget* widget = m_CapturingWidget.Get(); widget; widget = widget->GetParent()) {
				widget->OnEvent(&mouseButtonEvent);
			}
			m_CapturingWidget = nullptr;
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

		} else if(inButton == KeyCode::KEY_D && bPressed) {
			m_bDrawDebugInfo = !m_bDrawDebugInfo;
			LayoutWidget::FindNearest(m_DebugOverlay)->SetVisibility(m_bDrawDebugInfo);
		}
	}

	void	DispatchMouseScrollEvent(float inScroll) {}

	void	DispatchOSWindowResizeEvent(float2 inWindowSize) {
		// Ignore minimized state for now
		if(inWindowSize == float2())
			return;

		ParentLayoutEvent layoutEvent;
		layoutEvent.constraints = Rect(inWindowSize);

		for(auto& [window, layer] : m_WidgetStack)
			window->OnEvent(&layoutEvent);

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
		LOGF("Widget tree: \n{}", ss.str());
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
		m_HoveredWidget = nullptr;
		m_CapturingWidget = nullptr;
		m_bResetState = true;
	}

	Point   GetMousePosLocal() const {
		auto out = m_MousePosGlobal;
		if(m_HoveredWidget) {
			if(auto* parent = m_HoveredWidget->FindParentOfClass<LayoutWidget>()) {
				if(auto* hitData = m_LastHitStack.Find(parent)) {
					out = hitData->hitPosLocal;
				}				
			}
		}
		return out;
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

		if(m_bDrawDebugInfo) {
			std::string str;
			auto sb = util::StringBuilder(&str);

			sb.Line("Frame time: {:5.2f}ms", m_LastFrameTimeMs);
			sb.Line("Opened window count: {}", m_WidgetStack.size());
			sb.Line("Timers count: {}", m_Timers.Size());
			sb.Line("Mouse pos global: {}", m_MousePosGlobal);
			sb.Line("Hovered window: {}", m_HoveredWindow ? m_HoveredWindow->GetDebugID() : "");
			sb.Line("Hovered widget: {}", m_HoveredWidget ? m_HoveredWidget->GetDebugID() : "");
			sb.Line("Capturing mouse widget: {}", m_CapturingWidget ? m_CapturingWidget->GetDebugID() : "");
			sb.Line("MousePosLocal: {}", GetMousePosLocal());
			sb.Line();

			if(m_HoveredWindow) {
				sb.Line("{} Hit stack: ", m_HoveredWindow->GetDebugID());
				sb.PushIndent();
				PrintHitStack(sb);
			}
			m_DebugOverlay->FindChildOfClass<Text>()->SetText(str);
		}

		// Draw views
		g_Renderer->ResetDrawLists();
		auto* frameDrawList = g_Renderer->GetFrameDrawList();
		frameDrawList->PushFont(m_Theme->GetDefaultFont(), g_DefaultFontSize);
		{
			OPTICK_EVENT("Drawing UI");

			DrawlistImpl drawList;
			drawList.RendererDrawList = frameDrawList;
			DrawEvent drawEvent;
			drawEvent.drawList = &drawList;
			drawEvent.theme = m_Theme.get();

			for(auto& [window, layer] : m_WidgetStack) {
				if(LayoutWidget::FindNearest(window.get())->IsVisible()) {
					window->OnEvent(&drawEvent);
				}
			}
		}
		const auto frameEndTimePoint = std::chrono::high_resolution_clock::now();

		++m_FrameNum;
		m_LastFrameTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTimePoint - frameStartTimePoint).count() / 1000.f;

		g_Renderer->RenderFrame(true);
		return true;
	}
	
	void Orphan(Widget* inWidget) override {
		// Remove child without deletion
		for(auto it = m_WidgetStack.begin(); it != m_WidgetStack.end(); ++it) {
			if(it->widget.get() == inWidget) {
				it->widget.release();
				m_WidgetStack.erase(it);
				break;
			}
		}
	}
	// From Widget::Orphan()
	void Orphan(Widget* inWidget, WidgetSlot) override { Orphan(inWidget); }

	void BringToFront(Widget* inWidget) override {
		// Bring the child just below the overlay layer		
	}

	void Parent(Widget* inWidget, Layer inLayer) override {
		// Insert the widget based on layer
		// Call OnParented()
		if(inLayer == Layer::Overlay) {
			m_WidgetStack.emplace_back(RootWidget(inWidget, inLayer));

		} else if(inLayer == Layer::Background) {
			m_WidgetStack.emplace_front(RootWidget(inWidget, inLayer));

		} else {
			for(auto it = m_WidgetStack.begin(); it != m_WidgetStack.end(); ++it) {
				// Insert the window before overlays
				if(it->layer == Layer::Overlay) {
					m_WidgetStack.insert(it, RootWidget(inWidget, inLayer));
					break;
				}
			}
		}
		inWidget->OnParented(this);

		if(auto* layout = LayoutWidget::FindNearest(inWidget)) {
			ParentLayoutEvent layoutEvent;
			layoutEvent.constraints = Rect(g_OSWindow->GetSize());
			layout->OnEvent(&layoutEvent);
		}
	}

	bool	OnEvent(IEvent* inEvent) final {

		

		// if(auto* popupEvent = inEvent->Cast<PopupEvent>()) {

		// 	if(popupEvent->Type == PopupEvent::Type::Open) {
		// 		auto  popup = popupEvent->Spawner->OnSpawn(m_MousePosGlobal, g_OSWindow->GetSize());
		// 		auto* ptr = popup.get();
		// 		popup->OnParented(this);
		// 		LOGF("Opened the popup {}", popup->GetDebugIDString());

		// 		for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {
		// 			auto* window = it->get();

		// 			if(window->HasAnyWindowFlags(WindowFlags::Overlay)) {
		// 				m_WindowStack.insert(it, std::move(popup));
		// 				break;
		// 			}
		// 		}
		// 		ParentLayoutEvent layoutEvent;
		// 		layoutEvent.Constraints = Rect(g_OSWindow->GetSize());
		// 		ptr->OnEvent(&layoutEvent);
		// 		// Because we open a new window on top of other windows, update hovering state			
		// 		DispatchMouseMoveEvent(m_MousePosGlobal);
		// 		return true;
		// 	}

		// 	if(popupEvent->Type == PopupEvent::Type::Close) {
		// 		for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {

		// 			if(it->get() == popupEvent->Popup) {
		// 				LOGF("Closed the popup {}", popupEvent->Popup->GetDebugIDString());
		// 				m_WindowStack.erase(it);
		// 				ResetState();
		// 				break;
		// 			}
		// 		}
		// 		return true;
		// 	}

		// 	if(popupEvent->Type == PopupEvent::Type::CloseAll) {
		// 		for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {

		// 			if(it->get()->IsA<PopupWindow>()) {
		// 				LOGF("Closed the popup {}", it->get()->GetDebugIDString());
		// 				m_WindowStack.erase(it);
		// 				ResetState();
		// 			}
		// 		}
		// 		return true;
		// 	}			
		// }
		return true;
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

	// Draw hitstack and mouse pos
	bool					m_bDrawDebugInfo = true;
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
	WeakPtr<Widget>			m_HoveredWidget;
	WeakPtr<Widget>			m_CapturingWidget;
	WeakPtr<Widget>			m_HoveredWindow;

	DebugOverlayWindow* 	m_DebugOverlay = nullptr;

	// Stack of windows, bottom are background windows and top are overlay windows
	// Other windows in the middle
	struct RootWidget {
		std::unique_ptr<Widget> widget;
		Layer 					layer;

		RootWidget(Widget* inWidget, Layer inLayer)
			: layer(inLayer) {
			widget.reset(inWidget);
		}
	};
	std::list<RootWidget>	m_WidgetStack;

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

UI::FrameState UI::Application::GetFrameState() {
	return g_Application->GetFrameStateImpl();
}

GAVAUI_END