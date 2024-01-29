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



class RootWindow;
class Application;
class ApplicationImpl;

constexpr u32	 g_TooltipDelayMs = 500;
constexpr u8	 g_DefaultFontSize = 13;

INativeWindow*	 g_OSWindow = nullptr;
Renderer*		 g_Renderer = nullptr;
ApplicationImpl* g_Application = nullptr;



/*
* Root background area of the OS window
*/
class RootWindow: public SingleChildContainer {
	DEFINE_CLASS_META(RootWindow, SingleChildContainer)
public:

	RootWindow()
		: Super(std::string(GetClassName()), 
				Application::Get()->GetTheme()->FindStyle<LayoutStyle>(std::string(GetClassName())))
	{ 
		Super::SetAxisMode(AxisModeExpand); 
	}

	bool OnEvent(IEvent* inEvent) override {
		return Super::OnEvent(inEvent);
	}
};

/*
* Overlay that is drawn over root window
*/
class DebugOverlayWindow: public SingleChildContainer {
	DEFINE_CLASS_META(DebugOverlayWindow, SingleChildContainer)
public:

	DebugOverlayWindow()
		: Super(std::string(GetClassName()), 
				Application::Get()->GetTheme()->FindStyle<LayoutStyle>(std::string(GetClassName())))
	{ 
		Super::SetAxisMode(AxisModeShrink); 
		Text = new UI::Text(this, "");
	}

	bool OnEvent(IEvent* inEvent) override {	

		if(auto* event = inEvent->As<DrawEvent>()) {
			if(!Super::IsVisible()) return true;
			event->DrawList->DrawRectFilled(Super::GetRect().Translate(event->ParentOriginGlobal).Expand(20.f), Color("#454545dd"));

			auto eventCopy = *event;
			eventCopy.ParentOriginGlobal += Super::GetOriginLocal();
			Super::DispatchToChildren(&eventCopy);
			return true;
		}
		return Super::OnEvent(inEvent);
	}

	Text* Text = nullptr;
};

enum class MouseState {
	Default,	// When hovering an empty space or widget without a tooltip
	Tooltip,	// After some time hovering
	Held,		// When pressed and holding
	Dragged,	// When dragged threshold crossed
	DragDrop,	// When DragDrop active
};

enum Modifiers {
	LeftShift,
	RightShift,
	LeftControl,
	RightControl,
	LeftAlt,
	RightAlt,
	CapsLock,
	Count,
};


/*
* Top object that handles input events and drawing
*/
class ApplicationImpl: public UI::Application {
public:

	void	Init() {
		OPTICK_EVENT("UI Init");
		g_Renderer = CreateRendererDX12();
		g_Renderer->Init(g_OSWindow);

		m_Theme = std::make_unique<Theme>();

		// Layout parameters for all widgets
		m_Theme->Layout("").Margins(5, 5).Paddings(0);
		m_Theme->Text("Text").Color("#ffffff").Size(g_DefaultFontSize);
		m_Theme->Finalize();

		RebuildFonts();

		m_Layers.RootWindow = std::make_unique<RootWindow>();
		m_Layers.RootWindow->SetSize(g_OSWindow->GetSize());

		m_Layers.DebugOverlayWindow = std::make_unique<DebugOverlayWindow>();
		m_Layers.DebugOverlayWindow->SetSize(g_OSWindow->GetSize());
		m_Layers.DebugOverlayWindow->SetVisibility(false);

		g_OSWindow->SetOnCursorMoveCallback([](float x, float y) { g_Application->HandleMouseMoveEvent(x, y); });
		g_OSWindow->SetOnMouseButtonCallback([](KeyCode inButton, bool bPressed) { g_Application->HandleMouseButtonEvent(inButton, bPressed); });
		g_OSWindow->SetOnMouseScrollCallback([](float inScroll) { g_Application->HandleMouseScrollEvent(inScroll); });
		g_OSWindow->SetOnWindowResizedCallback([](float2 inWindowSize) { g_Application->HandleNativeWindowResizeEvent(inWindowSize); });
		g_OSWindow->SetOnKeyboardButtonCallback([](KeyCode inButton, bool bPressed) { g_Application->HandleKeyInputEvent(inButton, bPressed); });
		g_OSWindow->SetOnCharInputCallback([](wchar_t inCharacter) { g_Application->HandleKeyCharInputEvent(inCharacter); });
	}

	void	HandleMouseMoveEvent(float x, float y) {
		const auto mousePosGlobal = Point(x, y);
		const auto mouseDelta = mousePosGlobal - m_LastMousePosGlobal;

		if(m_MouseState == MouseState::Default || m_MouseState == MouseState::Tooltip) {
			HitStack hitStack;

			HitTestEvent hitTest;
			hitTest.HitPosGlobal = mousePosGlobal;
			hitTest.HitStack = &hitStack;

			auto bHandled = false;

			if(m_Layers.Popup) {
				m_Layers.Popup->OnEvent(&hitTest);

				if(!hitStack.Empty()) {
					bHandled = true;
				}
			}

			if(!bHandled) {
				m_Layers.RootWindow->OnEvent(&hitTest);

				if(!hitStack.Empty()) {
					bHandled = true;
				}
			}

			if(hitStack.Empty()) return;
			Widget* hovered = nullptr;

			// Notify last widget in the stack that it's hovered			
			HoverEvent hoverEvent{true};

			for(auto& hit : hitStack) {
				auto bHandled = hit.Widget->OnEvent(&hoverEvent);

				if(bHandled) {
					hovered = hit.Widget;
					break;
				}
			}

			// Update tooltip when hovered item has changed
			if(hovered != m_LastHovered) {
				m_Layers.Tooltip.Timer.Reset(g_TooltipDelayMs);

				if(m_MouseState == MouseState::Tooltip) {
					m_Layers.Tooltip.Widget.reset();
					m_MouseState = MouseState::Default;
				}
			}

			// Notify previous last widget in the stack that it's no longer hovered
			if(m_LastHovered && hovered != m_LastHovered) {
				HoverEvent hoverEvent{false};
				m_LastHovered->OnEvent(&hoverEvent);
			}
			m_LastHovered = hovered;
			m_LastHitStack = hitStack;

		} else if(m_MouseState == MouseState::Held) {

			if(m_CapturesMouse) {
				const auto mousePosGlobal = float2(x, y);				
				const auto mouseDeltaFromInitial = mousePosGlobal - m_MousePosOnCaptureGlobal;
				auto mousePosLocal = mousePosGlobal;

				// Convert global to local using hit test data
				const auto parentHitData = m_LastHitStack.Find(m_CapturesMouse->GetParent<LayoutWidget>());

				if(parentHitData.has_value()) {
					mousePosLocal = parentHitData->HitPosLocal + mouseDeltaFromInitial;
				}
				//LOGF("Mouse delta {}", mouseDelta);
				//LOGF("Mouse pos {}", mousePosLocal);

				MouseDragEvent dragEvent;
				dragEvent.InitPosLocal = mousePosLocal - mouseDeltaFromInitial;
				dragEvent.PosLocal = mousePosLocal;
				dragEvent.Delta = mouseDelta;
				m_CapturesMouse->OnEvent(&dragEvent);
			}
		}
		m_LastMousePosGlobal = {x, y};
	}

	void	HandleMouseButtonEvent(KeyCode inButton, bool bPressed) {

		if(inButton == KeyCode::KEY_LEFT_SHIFT) m_ModifiersState[LeftShift] = !m_ModifiersState[LeftShift];
		if(inButton == KeyCode::KEY_RIGHT_SHIFT) m_ModifiersState[RightShift] = !m_ModifiersState[RightShift];

		if(inButton == KeyCode::KEY_LEFT_CONTROL) m_ModifiersState[LeftControl] = !m_ModifiersState[LeftControl];
		if(inButton == KeyCode::KEY_RIGHT_CONTROL) m_ModifiersState[RightControl] = !m_ModifiersState[RightControl];

		if(inButton == KeyCode::KEY_LEFT_ALT) m_ModifiersState[LeftAlt] = !m_ModifiersState[LeftAlt];
		if(inButton == KeyCode::KEY_RIGHT_ALT) m_ModifiersState[RightAlt] = !m_ModifiersState[RightAlt];

		if(inButton == KeyCode::KEY_CAPS_LOCK) m_ModifiersState[CapsLock] = !m_ModifiersState[CapsLock];

		bPressed ? ++m_MouseButtonHeldNum : --m_MouseButtonHeldNum;

		if(m_MouseState == MouseState::Tooltip) {
			m_Layers.Tooltip.Widget.reset();
			m_MouseState = MouseState::Default;
		}

		// When pressed first time from default
		if(m_MouseState == MouseState::Default && bPressed && m_MouseButtonHeldNum == 1) {

			if(!m_LastHitStack.Empty()) {
				MouseButtonEvent event;
				event.MousePosGlobal = m_LastMousePosGlobal;
				event.bButtonPressed = bPressed;
				event.Button = (MouseButtonEnum)inButton;

				for(auto& hit: m_LastHitStack) {
					event.MousePosLocal = hit.HitPosLocal;
					auto bHandled = hit.Widget->OnEvent(&event);

					if(bHandled) {
						m_CapturesMouse = hit.Widget;
						m_MousePosOnCaptureGlobal = m_LastMousePosGlobal;
						m_MouseState = MouseState::Held;
						break;
					}
				}
			}

		} else if(!bPressed && !m_MouseButtonHeldNum) {
			m_CapturesMouse = nullptr;
			m_MouseState = MouseState::Default;
		}
	}

	void	HandleKeyInputEvent(KeyCode inButton, bool bPressed) {

		if(m_MouseState == MouseState::Tooltip) {
			m_Layers.Tooltip.Widget.reset();
		}

		if(inButton == KeyCode::KEY_P && bPressed) {
			LogWidgetTree();
		} else if(inButton == KeyCode::KEY_D && bPressed) {
			m_bDrawDebugInfo = !m_bDrawDebugInfo;
			m_Layers.DebugOverlayWindow->SetVisibility(m_bDrawDebugInfo);
		}
	}

	void	HandleMouseScrollEvent(float inScroll) {}

	void	HandleNativeWindowResizeEvent(float2 inWindowSize) {
		// Ignore minimized state for now
		if(inWindowSize == float2()) return;

		ParentLayoutEvent layoutEvent;
		layoutEvent.Constraints = inWindowSize;
		layoutEvent.bAxisChanged = {true, true};

		m_Layers.RootWindow->OnEvent(&layoutEvent);
		m_Layers.DebugOverlayWindow->OnEvent(&layoutEvent);

		g_Renderer->ResizeFramebuffers(inWindowSize);
	}

	void	HandleKeyCharInputEvent(wchar_t inChar) {}

	// Draws debug hit stack
	void	PrintHitStack(Util::StringBuilder& inBuffer) {
		Debug::PropertyArchive ar;

		for(auto& hitData : m_LastHitStack) {
			ar.PushObject(hitData.Widget->GetClassName(), hitData.Widget, nullptr);
			hitData.Widget->DebugSerialize(ar);
		}

		for(auto& object : ar.m_RootObjects) {
			inBuffer.Line(object->m_ClassName);
			inBuffer.PushIndent();

			for(auto& property : object->m_Properties) {
				inBuffer.Line("{}: {}", property.Name, property.Value);
			}
			inBuffer.PopIndent();
		}
	}

	void	DrawDebug(DrawList* inDrawList) {
		std::string buf;
		auto sb = Util::StringBuilder(&buf);

		sb.Line("FrameTime: {:5.2f}ms", m_LastFrameTimeMs);
		sb.Line("MousePosGlobal: {}", m_LastMousePosGlobal);
		sb.Line("MousePosLocal: {}", GetLocalHoveredPos());
		sb.Line("HitStack: ");
		sb.PushIndent();
		PrintHitStack(sb);

		m_Layers.DebugOverlayWindow->Text->SetText(buf);
	}

	Point	GetLocalHoveredPos() {
		if(m_LastHovered) {
			return m_LastHitStack.Find(m_LastHovered->As<LayoutWidget>())->HitPosLocal;
		}
		return m_LastMousePosGlobal;
	}

	void	LogWidgetTree() {
		Debug::PropertyArchive archive;
		DebugLogEvent onDebugLog;
		onDebugLog.Archive = &archive;

		m_Layers.RootWindow->OnEvent(&onDebugLog);

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
			printIndent(indent - 1); ss << "|-> " << inObject.m_ClassName << ":\n";

			for(auto& property : inObject.m_Properties) {
				printIndent(indent);
				ss << std::format("{}: {}", property.Name, property.Value) << '\n';
			}
			return true;
			};
		archive.VisitRecursively(visitor);
		LOGF("Widget tree: \n{}", ss.str());
	}

	Widget* GetRootWindow() { return m_Layers.RootWindow.get(); }

	Font*	GetDefaultFont() { return m_Theme->GetDefaultFont(); }

	// Rebuild font of the theme if needed
	void	RebuildFonts() {
		std::vector<Font*> fonts;
		m_Theme->GetFonts(&fonts);

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

public:

	bool	Tick() override {
		OPTICK_FRAME("UI_Tick");
		const auto frameStartTimePoint = std::chrono::high_resolution_clock::now();

		// Rebuild fonts if needed for different size
		{
			OPTICK_EVENT("Rebuilding fonts");		
			RebuildFonts();
		}

		// Update layout of new widgets
		// Process input events
		// Update layout of changed widgets
		// Draw
		{
			OPTICK_EVENT("Polling events");
			if(!g_OSWindow->PollEvents()) {
				return false;
			}
		}
		
		// Process tooltip
		if(m_MouseState == MouseState::Default && m_Layers.Tooltip.Timer.IsReady()) {

			m_LastHitStack.Top().Widget->VisitParent([&](Widget* inWidget) {

				if(auto* tooltipSpawner = inWidget->As<TooltipSpawner>()) {
					m_Layers.Tooltip.Widget.reset(tooltipSpawner->Spawn());
					m_Layers.Tooltip.Widget->SetPos(m_LastMousePosGlobal);
					m_MouseState = MouseState::Tooltip;
					return false;
				}
				return true;
			});
			m_Layers.Tooltip.Timer.Clear();
		}

		// Draw views
		g_Renderer->ResetDrawLists();
		auto& frameDrawList = *g_Renderer->GetFrameDrawList();
		frameDrawList.PushFont(m_Theme->GetDefaultFont(), g_DefaultFontSize);

		DrawEvent drawEvent;
		drawEvent.DrawList = &frameDrawList;
		drawEvent.ParentOriginGlobal = Point(0.f, 0.f);
		drawEvent.Theme = m_Theme.get();

		{
			OPTICK_EVENT("Drawing UI");

			m_Layers.RootWindow->OnEvent(&drawEvent);

			if(m_Layers.Popup) {
				m_Layers.Popup->OnEvent(&drawEvent);
			}

			if(m_Layers.DragDrop) {
				m_Layers.DragDrop->OnEvent(&drawEvent);
			}

			if(m_Layers.DebugOverlayWindow->IsVisible()) {
				m_Layers.DebugOverlayWindow->OnEvent(&drawEvent);
			}

			if(m_Layers.Tooltip.Widget) {
				m_Layers.Tooltip.Widget->OnEvent(&drawEvent);
			}

			if(m_bDrawDebugInfo) {
				DrawDebug(&frameDrawList);
			}
		}
		const auto frameEndTimePoint = std::chrono::high_resolution_clock::now();

		++m_FrameNum;
		m_LastFrameTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTimePoint - frameStartTimePoint).count() / 1000.f;

		{
			OPTICK_EVENT("Rendering UI vsync");
			g_Renderer->RenderFrame(true);
		}
		return true;
	}

	// Returns top level root widget containter
	Widget* GetRoot() override { return m_Layers.RootWindow.get(); }

	void	Shutdown() override {}

	Theme*	GetTheme() override { return m_Theme.get(); }

	void	SetTheme(Theme* inTheme) override { 
		m_Theme->Merge(inTheme);
		m_Theme->Finalize();
		// Add our default font size
		m_Theme->GetDefaultFont()->RasterizeFace(g_DefaultFontSize);
		RebuildFonts();
	}

private:

	u64						m_FrameNum = 0;
	float					m_LastFrameTimeMs = 0;

	// Draw hitstack and mouse pos
	bool					m_bDrawDebugInfo = false;
	// Whether the native window is minimized
	bool					m_bMinimized = false;

	Point					m_LastMousePosGlobal;
	Widget*					m_LastHovered = nullptr;
	// A widget which has received a mouse click event
	Widget*					m_CapturesMouse = nullptr;

	HitStack				m_LastHitStack;

	// Number of buttons currently held
	u8						m_MouseButtonHeldNum = 0;
	MouseState				m_MouseState = MouseState::Default;
	// Position of the mouse cursor when button has been pressed
	Point					m_MousePosOnCaptureGlobal;

	bool					m_ModifiersState[(int)Modifiers::Count] = {false};

	struct {
		// Tooltip drawn on top of all other widgets
		// to allow tooltips over debug overlays
		struct {
			std::unique_ptr<LayoutWidget>	Widget;
			Timer							Timer;
		}								Tooltip;

		std::unique_ptr<DebugOverlayWindow>	DebugOverlayWindow;
		std::unique_ptr<LayoutWidget>		Popup;
		std::unique_ptr<LayoutWidget>		DragDrop;
		// Currently unused
		//std::list<Widget*>	FloatingWindowsStack;
		std::unique_ptr<RootWindow>		RootWindow;
	}						m_Layers;

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

GAVAUI_END