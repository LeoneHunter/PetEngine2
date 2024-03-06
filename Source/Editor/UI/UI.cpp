#include "UI.h"
#include "Widget.h"
#include "Runtime/Platform/Window.h"
#include "Runtime/System/Renderer/UIRenderer.h"
#include "Runtime/System/JobDispatcher.h"

#include "ThirdParty/Optik/include/optick.config.h"
#include "ThirdParty/Optik/include/optick.h"

#include <stack>

#define GAVAUI_BEGIN namespace UI {
#define GAVAUI_END }

GAVAUI_BEGIN

using Timer = Util::Timer<std::chrono::milliseconds>;
using RendererDrawlist = ::DrawList;

class WindowController;
class Application;
class ApplicationImpl;

constexpr u32	 g_TooltipDelayMs = 500;
constexpr u8	 g_DefaultFontSize = 13;
constexpr bool	 g_DrawCliprects = true;

INativeWindow*	 g_OSWindow = nullptr;
Renderer*		 g_Renderer = nullptr;
ApplicationImpl* g_Application = nullptr;


namespace Names {
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
	DrawList*	m_DrawList;
	Color		m_Color;
	float		m_Indent;
};



class DrawlistImpl final: public Drawlist {
public:

	void PushBox(Rect inRect, const BoxStyle* inStyle) override {
		Rect backgroundRect;
		Rect borderRect;

		if(inStyle->BorderAsOutline) {
			backgroundRect = inRect;
			borderRect = Rect(backgroundRect).Expand(inStyle->Borders);
		} else {
			borderRect = inRect;
			backgroundRect = Rect(inRect).Expand(
				(float)-inStyle->Borders.Top, 
				(float)-inStyle->Borders.Right, 
				(float)-inStyle->Borders.Bottom, 
				(float)-inStyle->Borders.Left);
		}

		auto borderColor = ColorFloat4(inStyle->BorderColor);
		auto backgroundColor = ColorFloat4(inStyle->BackgroundColor);

		borderColor.a *= inStyle->Opacity;
		backgroundColor.a *= inStyle->Opacity;

		if(inStyle->Borders.Left || inStyle->Borders.Right || inStyle->Borders.Top || inStyle->Borders.Bottom) {

			if(backgroundColor.a != 1.f) {
				RendererDrawList->DrawRect(borderRect, ColorU32(borderColor), inStyle->Rounding, (DrawList::Corner)inStyle->RoundingMask);
			} else {
				RendererDrawList->DrawRectFilled(borderRect, ColorU32(borderColor), inStyle->Rounding, (DrawList::Corner)inStyle->RoundingMask);
			}
		}

		RendererDrawList->DrawRectFilled(backgroundRect, ColorU32(backgroundColor), inStyle->Rounding, (DrawList::Corner)inStyle->RoundingMask);
	}

	void PushBox(Rect inRect, Color inColor, bool bFilled = true) override {

		if(bFilled) {
			RendererDrawList->DrawRectFilled(inRect, ColorU32(inColor));
		} else {
			RendererDrawList->DrawRect(inRect, ColorU32(inColor));
		}
	}

	void PushText(Point inOrigin, const TextStyle* inStyle, std::string_view inTextView) override {
		RendererDrawList->DrawText(inOrigin, inStyle->Color, inTextView, inStyle->FontSize, inStyle->FontWeightBold, inStyle->FontStyleItalic);
	}

	void PushClipRect(Rect inClipRect) override {
		RendererDrawList->PushClipRect(inClipRect);

		if(g_DrawCliprects) {
			RendererDrawList->DrawRect(inClipRect.Expand(-1), Colors::Red);
		}
	}

	void PopClipRect() override {
		RendererDrawList->PopClipRect();
	}

	void PushTransform(float2 inTransform) override {
		RendererDrawList->PushTransform(inTransform);
	}

	void PopTransform() {
		RendererDrawList->PopTransform();
	}
		
	RendererDrawlist*	RendererDrawList = nullptr;
};


/*
* Overlay that is drawn over root window
*/
class DebugOverlayWindow: public Window {
public:

	DebugOverlayWindow(Application* inApp)
		: Window(
			inApp, 
			Names::DebugOverlayWindowID, 
			WindowFlags::Overlay, 
			Names::DebugOverlayWindowID)
	{ 
		Super::SetAxisMode(AxisModeShrink);
		Super::SetVisibility(false);
		Super::SetOrigin(GetLayoutInfo().Margins.TL());

		new UI::Text("", this);
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

	void	Init() {
		OPTICK_EVENT("UI Init");
		g_Renderer = CreateRendererDX12();
		g_Renderer->Init(g_OSWindow);

		m_Theme.reset(Theme::DefaultLight());

		auto& debugOverlayStyle = m_Theme->Add(Names::DebugOverlayWindowID);
		debugOverlayStyle.Add<LayoutStyle>().SetMargins(5, 5).SetPaddings(5, 5);
		debugOverlayStyle.Add<BoxStyle>().SetFillColor("#454545dd").SetRounding(4);

		RebuildFonts();

		m_DebugOverlay = new DebugOverlayWindow(g_Application);

		g_OSWindow->SetOnCursorMoveCallback([](float x, float y) { g_Application->DispatchMouseMoveEvent({x, y}); });
		g_OSWindow->SetOnMouseButtonCallback([](KeyCode inButton, bool bPressed) { g_Application->DispatchMouseButtonEvent(inButton, bPressed); });
		g_OSWindow->SetOnMouseScrollCallback([](float inScroll) { g_Application->DispatchMouseScrollEvent(inScroll); });
		g_OSWindow->SetOnWindowResizedCallback([](float2 inWindowSize) { g_Application->DispatchOSWindowResizeEvent(inWindowSize); });
		g_OSWindow->SetOnKeyboardButtonCallback([](KeyCode inButton, bool bPressed) { g_Application->DispatchKeyEvent(inButton, bPressed); });
		g_OSWindow->SetOnCharInputCallback([](wchar_t inCharacter) { g_Application->DispatchCharInputEvent(inCharacter); });

		DispatchOSWindowResizeEvent(g_OSWindow->GetSize());
	}

	void	DispatchMouseMoveEvent(Point inMousePosGlobal) {
		const auto mousePosGlobal = inMousePosGlobal;
		const auto mouseDelta = mousePosGlobal - m_MousePosGlobal;

		MouseDragEvent mouseMoveEvent;
		mouseMoveEvent.MousePosOnCaptureLocal = m_MousePosOnCaptureGlobal;
		mouseMoveEvent.MousePosLocal = mousePosGlobal;
		mouseMoveEvent.MouseDelta = mouseDelta;
		mouseMoveEvent.MouseButtonsPressedBitField = m_MouseButtonsPressedBitField;

		if(mousePosGlobal == NOPOINT && m_HoveredWidget) {
			HoverEvent hoverEvent{false};
			auto* outermostWrapper = m_HoveredWidget->GetOutermostWrapper();
			outermostWrapper
				? outermostWrapper->OnEvent(&hoverEvent)
				: m_HoveredWidget->OnEvent(&hoverEvent);
			m_HoveredWidget = nullptr;

		} else if(m_CapturingWidget) {
			const auto mouseDeltaFromInitial = mousePosGlobal - m_MousePosOnCaptureGlobal;
			// Convert global to local using hit test data
			const auto* parentHitData = m_LastHitStack.Find(m_CapturingWidget->GetParent<LayoutWidget>());
			const auto  mousePosLocal = parentHitData 
				? parentHitData->HitPosLocal + mouseDeltaFromInitial 
				: mousePosGlobal;

			MouseDragEvent dragEvent;
			dragEvent.MousePosOnCaptureLocal = mousePosLocal - mouseDeltaFromInitial;
			dragEvent.MousePosLocal = mousePosLocal;
			dragEvent.MouseDelta = mouseDelta;
			m_CapturingWidget->OnEvent(&dragEvent);

		} else {
			LayoutWidget*	hoveredWidget = nullptr;
			Window*			hoveredWindow = nullptr;
			HitTestEvent	hitTest;
			hitTest.HitPosGlobal = mousePosGlobal;

			for(auto it = --m_WindowStack.end(); it != --m_WindowStack.begin(); --it) {
				auto* window = it->get();

				if(window->HasAnyWindowFlags(WindowFlags::Overlay)) 
					continue;
				const auto bHovered = window->OnEvent(&hitTest);

				if(bHovered) {					
					m_LastHitStack = hitTest.HitStack;
					hoveredWindow = window;
					break;
				}
			}

			if(hoveredWindow) {				
				HoverEvent hoverEvent{true};

				for(auto& [widget, hitPos] : m_LastHitStack) {
					auto* outermostWrapper = widget->GetOutermostWrapper();

					auto bHandled = outermostWrapper
						? outermostWrapper->OnEvent(&hoverEvent)
						: widget->OnEvent(&hoverEvent);

					if(bHandled && widget) {
						LOGF("Dispatched hover event. Target {}", widget->GetDebugIDString());
						hoveredWidget = *widget;
						break;
					}
				}
			}

			if(m_HoveredWidget && m_HoveredWidget != hoveredWidget) {
				HoverEvent hoverEvent{false};
				auto* outermostWrapper = m_HoveredWidget->GetOutermostWrapper();
				outermostWrapper 
					? outermostWrapper->OnEvent(&hoverEvent) 
					: m_HoveredWidget->OnEvent(&hoverEvent);
			}

			m_HoveredWidget = hoveredWidget->GetWeak();
			m_HoveredWindow = hoveredWindow->GetWeak();
		}		
		m_MousePosGlobal = mousePosGlobal;
	}

	void	DispatchMouseButtonEvent(KeyCode inButton, bool bPressed) {

		bPressed ? ++m_MouseButtonHeldNum : --m_MouseButtonHeldNum;
		bPressed ? m_MouseButtonsPressedBitField |= (MouseButtonMask)inButton 
				 : m_MouseButtonsPressedBitField &= ~(MouseButtonMask)inButton;

		// When pressed first time from default
		if(bPressed && m_MouseButtonHeldNum == 1) {
			m_PressedMouseButton = (MouseButton)inButton;

			if(m_HoveredWidget) {
				MouseButtonEvent event;
				event.MousePosGlobal = m_MousePosGlobal;
				event.MousePosLocal = m_MousePosGlobal;
				event.bButtonPressed = bPressed;
				event.Button = (MouseButton)inButton;

				for(auto& [widget, pos]: m_LastHitStack) {
					// In case widget has been deleted between events
					if(!widget) continue;

					const auto* parent = widget->GetParent<LayoutWidget>();
					// Parent hit data could be null because some widgets could have parents that are not hovered
					// Like a Popup stack when a subpopup has a parent but it's not hovered
					const auto* parentHitData = m_LastHitStack.Find(parent);
					event.MousePosLocal = parentHitData ? parentHitData->HitPosLocal : m_MousePosGlobal;

					auto* wrapper = widget->GetOutermostWrapper();
					auto bHandled = wrapper ? wrapper->OnEvent(&event) : widget->OnEvent(&event);

					// A widget could be deleted in the OnEvent call
					if(bHandled && widget) {
						m_CapturingWidget = widget;
						m_MousePosOnCaptureGlobal = m_MousePosGlobal;
						break;
					}
				}
			}
			return;
		} 
		
		if(m_CapturingWidget && !bPressed && (MouseButton)inButton == m_PressedMouseButton) {
			MouseButtonEvent mouseButtonEvent;
			mouseButtonEvent.bButtonPressed = bPressed;
			mouseButtonEvent.Button = (MouseButton)inButton;
			mouseButtonEvent.MousePosGlobal = m_MousePosGlobal;

			auto* wrapper = m_CapturingWidget->GetOutermostWrapper();
			wrapper ? wrapper->OnEvent(&mouseButtonEvent) : m_CapturingWidget->OnEvent(&mouseButtonEvent);

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
			
			for(auto windowIt = m_WindowStack.rbegin(); windowIt != m_WindowStack.rend(); ++windowIt) {
				auto* window = windowIt->get();
				LogWidgetTree(window);
			}

		} else if(inButton == KeyCode::KEY_D && bPressed) {
			m_bDrawDebugInfo = !m_bDrawDebugInfo;
			m_DebugOverlay->SetVisibility(m_bDrawDebugInfo);
		}
	}

	void	DispatchMouseScrollEvent(float inScroll) {}

	void	DispatchOSWindowResizeEvent(float2 inWindowSize) {
		// Ignore minimized state for now
		if(inWindowSize == float2()) 
			return;

		ParentLayoutEvent layoutEvent;
		layoutEvent.Constraints = Rect(inWindowSize);

		for(auto& window : m_WindowStack) 
			window->OnEvent(&layoutEvent);

		g_Renderer->ResizeFramebuffers(inWindowSize);
	}

	void	DispatchCharInputEvent(wchar_t inChar) {}

	void	LogWidgetTree(Window* inWindow) {
		Debug::PropertyArchive archive;
		DebugLogEvent onDebugLog;
		onDebugLog.Archive = &archive;

		inWindow->OnEvent(&onDebugLog);

		std::deque<Debug::PropertyArchive::ObjectID> parentIDStack;
		std::stringstream ss;

		auto printIndent = [&](size_t inIndent) {
			if(!inIndent) return;
			ss << "    ";
			--inIndent;

			for(; inIndent; --inIndent) {
				ss << "|   ";
			}
		};

		auto visitor = [&](Debug::PropertyArchive::Object& inObject)->bool {
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

	void	PrintHitStack(Util::StringBuilder& inBuffer) {
		Debug::PropertyArchive ar;

		for(auto& hitData : m_LastHitStack) {
			ar.PushObject(hitData.Widget->GetDebugIDString(), *hitData.Widget, nullptr);
			hitData.Widget->DebugSerialize(ar);
		}

		for(auto it = ar.m_RootObjects.rbegin(); it != ar.m_RootObjects.rend(); ++it) {
			auto& object = *it;
			inBuffer.Line(object->m_DebugName);
			inBuffer.PushIndent();

			for(auto& property : object->m_Properties) {
				inBuffer.Line("{}: {}", property.Name, property.Value);
			}
			inBuffer.PopIndent();
		}
	}

	// Update cached widgets for mouse capture and hovering
	void	ResetState() {
		m_HoveredWindow = nullptr;
		m_HoveredWidget = nullptr;
		m_CapturingWidget = nullptr;
		m_bResetState = true;
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
			auto sb = Util::StringBuilder(&str);

			sb.Line("Frame time: {:5.2f}ms", m_LastFrameTimeMs);
			sb.Line("Opened window count: {}", m_WindowStack.size());	
			sb.Line("Timers count: {}", m_Timers.Size());
			sb.Line("Mouse pos global: {}", m_MousePosGlobal);
			sb.Line("Hovered window: {}", m_HoveredWindow ? m_HoveredWindow->GetDebugIDString() : "");
			sb.Line("Hovered widget: {}", m_HoveredWidget ? m_HoveredWidget->GetDebugIDString() : "");
			sb.Line("Capturing mouse widget: {}", m_CapturingWidget ? m_CapturingWidget->GetDebugIDString() : "");
			//sb.Line("MousePosLocal: {}", GetLocalHoveredPos());
			sb.Line();

			if(m_HoveredWindow) {
				sb.Line("{} Hit stack: ", m_HoveredWindow->GetDebugIDString());
				sb.PushIndent();
				PrintHitStack(sb);
			}
			m_DebugOverlay->GetChild<Text>()->SetText(str);
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
			drawEvent.DrawList = &drawList;
			drawEvent.Theme = m_Theme.get();

			for(auto& window : m_WindowStack) {

				if(window->IsVisible()) {
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

	void	ParentWindow(Window* inWindow) final {
		Assertm(!inWindow->HasAnyWindowFlags(WindowFlags::Popup), "Popup windows should be created through PopupSpawner");

		Assertm(m_WindowStack.empty() ||
			!m_WindowStack.front()->HasAnyWindowFlags(WindowFlags::Background) ||
			!inWindow->HasAnyWindowFlags(WindowFlags::Background),
			"There's already a background window. Only one window can be a background window");

		if(inWindow->HasAnyWindowFlags(WindowFlags::Overlay)) {
			m_WindowStack.emplace_back(inWindow);

		} else if(inWindow->HasAnyWindowFlags(WindowFlags::Background)) {
			m_WindowStack.emplace_front(inWindow);

		} else {
			for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {
				// Insert the window before overlays
				if(it->get()->HasAnyWindowFlags(WindowFlags::Overlay)) {
					m_WindowStack.insert(it, std::unique_ptr<Window>(inWindow));
					break;
				}
			}
		}
		inWindow->OnParented(this);

		ParentLayoutEvent layoutEvent;
		layoutEvent.Constraints = Rect(g_OSWindow->GetSize());
		inWindow->OnEvent(&layoutEvent);		
	}

	bool	OnEvent(IEvent* inEvent) final {

		if(auto* spawnTooltipIntent = inEvent->Cast<SpawnTooltipIntent>()) {
			auto tooltip = spawnTooltipIntent->Spawner->OnSpawn(m_MousePosGlobal);
			tooltip->OnParented(this);
			LOGF("Opened the tooltip {}", tooltip->GetDebugIDString());
			m_WindowStack.push_back(std::move(tooltip));
			return true;
		}

		if(auto* closeTooltipIntent = inEvent->Cast<CloseTooltipIntent>()) {
			for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {

				if(it->get() == closeTooltipIntent->Tooltip) {
					LOGF("Closed the tooltip {}", closeTooltipIntent->Tooltip->GetDebugIDString());
					m_WindowStack.erase(it);
					break;
				}
			}
			return true;
		}

		if(auto* popupEvent = inEvent->Cast<PopupEvent>()) {

			if(popupEvent->Type == PopupEvent::Type::Open) {
				auto  popup = popupEvent->Spawner->OnSpawn(m_MousePosGlobal, g_OSWindow->GetSize());
				auto* ptr = popup.get();
				popup->OnParented(this);
				LOGF("Opened the popup {}", popup->GetDebugIDString());

				for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {
					auto* window = it->get();

					if(window->HasAnyWindowFlags(WindowFlags::Overlay)) {
						m_WindowStack.insert(it, std::move(popup));
						break;
					}
				}
				ParentLayoutEvent layoutEvent;
				layoutEvent.Constraints = Rect(g_OSWindow->GetSize());
				ptr->OnEvent(&layoutEvent);
				// Because we open a new window on top of other windows, update hovering state			
				DispatchMouseMoveEvent(m_MousePosGlobal);
				return true;
			}

			if(popupEvent->Type == PopupEvent::Type::Close) {
				for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {

					if(it->get() == popupEvent->Popup) {
						LOGF("Closed the popup {}", popupEvent->Popup->GetDebugIDString());
						m_WindowStack.erase(it);
						ResetState();
						break;
					}
				}
				return true;
			}

			if(popupEvent->Type == PopupEvent::Type::CloseAll) {
				for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {

					if(it->get()->IsA<PopupWindow>()) {
						LOGF("Closed the popup {}", it->get()->GetDebugIDString());
						m_WindowStack.erase(it);
						ResetState();
					}
				}
				return true;
			}			
		}
		return true;
	}

	void	Shutdown() final {}

	Theme*	GetTheme() final {
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
		state.KeyModifiersState = m_ModifiersState;
		state.MouseButtonHeldNum = m_MouseButtonHeldNum;
		state.MouseButtonsPressedBitField = m_MouseButtonsPressedBitField;
		state.MousePosGlobal = m_MousePosGlobal;
		state.MousePosOnCaptureGlobal = m_MousePosOnCaptureGlobal;
		state.WindowSize = g_OSWindow->GetSize();
		state.Theme = m_Theme.get();
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
	bool					m_bDrawDebugInfo = false;
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
	
	HitStack				m_LastHitStack;
	WeakPtr<LayoutWidget>	m_HoveredWidget;
	WeakPtr<LayoutWidget>	m_CapturingWidget;
	WeakPtr<Window>			m_HoveredWindow;	

	DebugOverlayWindow*		m_DebugOverlay = nullptr;

	// Stack of windows, bottom are background windows and top are overlay windows
	// Other windows in the middle
	std::list<std::unique_ptr<Window>>		
							m_WindowStack;

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