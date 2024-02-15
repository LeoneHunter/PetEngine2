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

	void PushBox(Rect inRect, const BoxStyle* inStyle) final {
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
				RendererDrawList->DrawRect(borderRect.Translate(Transform), ColorU32(borderColor), inStyle->Rounding, (DrawList::Corner)inStyle->RoundingMask);
			} else {
				RendererDrawList->DrawRectFilled(borderRect.Translate(Transform), ColorU32(borderColor), inStyle->Rounding, (DrawList::Corner)inStyle->RoundingMask);
			}
		}

		RendererDrawList->DrawRectFilled(backgroundRect.Translate(Transform), ColorU32(backgroundColor), inStyle->Rounding, (DrawList::Corner)inStyle->RoundingMask);
	}

	void PushBox(Rect inRect, Color inColor, bool bFilled = true) final {

		if(bFilled) {
			RendererDrawList->DrawRectFilled(inRect.Translate(Transform), ColorU32(inColor));
		} else {
			RendererDrawList->DrawRect(inRect.Translate(Transform), ColorU32(inColor));
		}
	}

	void PushText(Point inOrigin, const TextStyle* inStyle, std::string_view inTextView) final {
		RendererDrawList->DrawText(inOrigin + Transform, inStyle->Color, inTextView, inStyle->FontSize, inStyle->FontWeightBold, inStyle->FontStyleItalic);
	}

	void PushClipRect(Rect inClipRect) final {
		RendererDrawList->PushClipRect(inClipRect.Translate(Transform));
		bHasClipRect = true;

		if(g_DrawCliprects) {
			RendererDrawList->DrawRect(inClipRect.Expand(-1), Colors::Red);
		}
	}

	void PopClipRect() {
		RendererDrawList->PopClipRect();
	}

	float2				Transform;
	RendererDrawlist*	RendererDrawList = nullptr;
	// When a widget pushes clip rect it applies to all children recursively
	// So we need to pop it on the way back up
	bool				bHasClipRect = false;
};


/*
* Overlay that is drawn over root window
*/
class DebugOverlayWindow: public Window {
public:

	DebugOverlayWindow(Application* inApp)
		: Window(inApp, Names::DebugOverlayWindowID, WindowFlags::Overlay)
	{ 
		Super::SetAxisMode(AxisModeShrink);
		Super::SetVisibility(false);
		m_Style = Application::Get()->GetTheme()->Find(Names::DebugOverlayWindowID);
		Super::SetOrigin(GetMargins().TL());

		Text = new UI::Text(ChildSlot, "");
	}

	bool OnEvent(IEvent* inEvent) override {

		if(auto* event = inEvent->Cast<DrawEvent>()) {
			event->DrawList->PushBox(Super::GetRect(), m_Style->Find<BoxStyle>());
		}
		return Window::OnEvent(inEvent);
	}

	Padding GetPaddings() const override { 
		return m_Style ? m_Style->Find<LayoutStyle>()->Paddings : Padding(); 
	}

	Margin  GetMargins() const override { 
		return m_Style ? m_Style->Find<LayoutStyle>()->Margins : Margin(); 
	}

	const StyleClass*	m_Style;
	Text*				Text = nullptr;
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
		debugOverlayStyle.Add<LayoutStyle>().SetMargins(5, 5);
		debugOverlayStyle.Add<BoxStyle>().SetFillColor("#454545dd").SetRounding(4);

		RebuildFonts();

		auto* debugWindow = new DebugOverlayWindow(g_Application);

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

		if(mousePosGlobal == NOPOINT) {

			if(m_HoveredWidget) {
				m_HoveredWidget->OnEvent(&mouseMoveEvent);
				m_HoveredWidget = nullptr;
			}
			m_HoveredWindow = nullptr;

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
			const auto bHasPopup = !m_PopupWindowsStack.empty();

			Widget* hoveredWidget = nullptr;
			Window* hoveredWindow = nullptr;
			HitTestEvent hitTest;
			hitTest.HitPosGlobal = mousePosGlobal;

			ForEachWindow([&](Window* inWindow) {	
				if(inWindow->HasAnyWindowFlags(WindowFlags::Overlay))
					return true;

				inWindow->OnEvent(&hitTest);

				if(!hitTest.HitStack.Empty()) {
					m_LastHitStack = hitTest.HitStack;
					hoveredWindow = inWindow;
					return false;
				}
				return true;
			}, false, !bHasPopup);

			if(hoveredWindow) {				
				HoverEvent hoverEvent{true};

				for(auto& hit : m_LastHitStack) {
					auto bHandled = hit.Widget->OnEvent(&hoverEvent);

					if(bHandled) {
						hoveredWidget = hit.Widget;
						break;
					}
				}								
			}	

			if(m_HoveredWidget && m_HoveredWidget != hoveredWidget) {
				HoverEvent hoverEvent{false};
				m_HoveredWidget->OnEvent(&hoverEvent);
			}

			if(bHasPopup) {
				// Check hit stack for SubPopup widgets
				// If such widget exists, check if it spawns the same popup as already opened
				// If not: close previous popup and open a new one
				// Else: ignore
			}
			m_HoveredWidget = hoveredWidget;
			m_HoveredWindow = hoveredWindow;					
		}		
		m_MousePosGlobal = mousePosGlobal;
	}

	void	DispatchMouseButtonEvent(KeyCode inButton, bool bPressed) {

		bPressed ? ++m_MouseButtonHeldNum : --m_MouseButtonHeldNum;
		bPressed ? m_MouseButtonsPressedBitField |= (MouseButtonMask)inButton 
				 : m_MouseButtonsPressedBitField &= ~(MouseButtonMask)inButton;

		//if(m_MouseState == MouseState::Tooltip) {
		//	m_Layers.Tooltip.Widget.reset();
		//	m_MouseState = MouseState::Default;
		//}

		// Close popups when not hitting a popup
		if(bPressed && !m_PopupWindowsStack.empty() && (!m_HoveredWindow || !m_HoveredWindow->HasAnyWindowFlags(WindowFlags::Popup))) {
			m_PopupWindowsStack.front().Spawner->OnDestroy();
			m_PopupWindowsStack.clear();
		}

		// When pressed first time from default
		if(bPressed && m_MouseButtonHeldNum == 1) {
			m_PressedMouseButton = (MouseButton)inButton;

			if(m_HoveredWidget) {
				MouseButtonEvent event;
				event.MousePosGlobal = m_MousePosGlobal;
				event.MousePosLocal = m_MousePosGlobal;
				event.bButtonPressed = bPressed;
				event.Button = (MouseButton)inButton;

				for(Widget* widget = m_LastHitStack.Top().Widget;; widget = widget->GetParent()) {

					if(widget->IsA<LayoutWidget>()) {
						const auto* parent = widget->GetParent<LayoutWidget>();
						event.MousePosLocal = parent ? m_LastHitStack.Find(parent)->HitPosLocal : m_MousePosGlobal;
					}

					if(widget->OnEvent(&event)) {
						m_CapturingWidget = widget;
						m_MousePosOnCaptureGlobal = m_MousePosGlobal;
						break;
					}
				}
			}

		} else if(!bPressed) {

			if((MouseButton)inButton == m_PressedMouseButton && m_CapturingWidget) {
				MouseButtonEvent mouseButtonEvent;
				mouseButtonEvent.bButtonPressed = bPressed;
				mouseButtonEvent.Button = (MouseButton)inButton;
				mouseButtonEvent.MousePosGlobal = m_MousePosGlobal;

				auto bHandled = m_CapturingWidget->OnEvent(&mouseButtonEvent);

				if(bHandled && m_HoveredWindow && m_HoveredWindow->HasAnyWindowFlags(WindowFlags::Popup)) {
					m_PopupWindowsStack.front().Spawner->OnDestroy();
					m_PopupWindowsStack.clear();
				}

				m_CapturingWidget = nullptr;
				m_MousePosOnCaptureGlobal = NOPOINT;
				m_PressedMouseButton = MouseButton::None;
			}			
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

		/*if(m_MouseState == MouseState::Tooltip) {
			m_Layers.Tooltip.Widget.reset();
		}*/

		if(inButton == KeyCode::KEY_P && bPressed) {
			
			for(auto windowIt = m_WindowStack.rbegin(); windowIt != m_WindowStack.rend(); ++windowIt) {
				auto* window = windowIt->get();
				LogWidgetTree(window);
			}

		} else if(inButton == KeyCode::KEY_D && bPressed) {
			m_bDrawDebugInfo = !m_bDrawDebugInfo;
			m_WindowStack.back()->SetVisibility(m_bDrawDebugInfo);
		}
	}

	void	DispatchMouseScrollEvent(float inScroll) {}

	void	DispatchOSWindowResizeEvent(float2 inWindowSize) {
		// Ignore minimized state for now
		if(inWindowSize == float2()) 
			return;

		ParentLayoutEvent layoutEvent;
		layoutEvent.Constraints = Rect(inWindowSize);			
		m_WindowStack.front()->OnEvent(&layoutEvent);
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

	// We will iterate a subtree manually and handle nesting and visibility
	// Possibly opens a possibility to cache draw commands
	// Also we will handle clip rects nesting
	void	DrawWindow(Window* inWindow, DrawList* inRendererDrawlist, Theme* inTheme) {

		struct NodeData {
			Widget*		Widget = nullptr;
			float2		GlobalOrigin = {0.f, 0.f};
			bool		bHasClipRect = false;
		};
		std::vector<NodeData> widgetStack;
		widgetStack.reserve(20);
		widgetStack.push_back(NodeData{inWindow, float2(0.f)});

		DrawlistImpl drawList;
		drawList.RendererDrawList = inRendererDrawlist;

		DrawEvent drawEvent;
		drawEvent.DrawList = &drawList;
		drawEvent.ParentOriginGlobal = Point(0.f, 0.f);
		drawEvent.Theme = inTheme;

		Widget* prevWidget = inWindow;

		if(auto* rootLayout = inWindow->Cast<LayoutWidget>()) {
			rootLayout->OnEvent(&drawEvent);
			widgetStack.push_back(NodeData{rootLayout, rootLayout->GetOrigin(), drawList.bHasClipRect});
			drawList.Transform = rootLayout->GetOrigin();
		}

		// Draws children depth first
		inWindow->VisitChildren([&](Widget* inWidget)->VisitResult {
			auto* layoutWidget = inWidget->Cast<LayoutWidget>();

			if(layoutWidget && !layoutWidget->IsVisible())
				return VisitResultSkipChildren;

			auto* prevParent = widgetStack.back().Widget;
			auto parent = inWidget->GetParent();

			// Check if we are going to a child, sibling or parent
			if(parent == inWindow) {
				// We are under root
			} else if(parent == prevWidget) {
				// We go down
				// If previous was a layout push it on stack
				if(auto* prevAsLayout = prevWidget->Cast<LayoutWidget>()) {
					const auto prevParentOrigin = widgetStack.back().GlobalOrigin;
					const auto prevOrigin = prevAsLayout->GetOrigin();
					const auto newTransform = prevParentOrigin + prevOrigin;
					widgetStack.push_back(NodeData{prevWidget, newTransform, drawList.bHasClipRect});
					drawList.Transform = newTransform;
				}

			} else if(parent == prevParent) {
				// We're a sibling
			} else {
				// We've returned somewhere up the stack
				// Unwind and pop transforms and clip rects
				for(auto nodeData = widgetStack.back(); nodeData.Widget != parent; widgetStack.pop_back(), nodeData = widgetStack.back()) {

					if(nodeData.bHasClipRect) {
						drawList.PopClipRect();
					}
				}
				drawList.Transform = widgetStack.back().GlobalOrigin;
			}
			drawList.bHasClipRect = false;

			if(layoutWidget) {
				layoutWidget->OnEvent(&drawEvent);
			}
			prevWidget = inWidget;
			return VisitResultContinue;

		}, true);

		for(auto it = widgetStack.rbegin(); it != widgetStack.rend(); ++it) {

			if(it->bHasClipRect) {
				drawList.PopClipRect();
			}
		}
	}

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

public:

	bool	Tick() final {
		OPTICK_FRAME("UI_Tick");
		const auto frameStartTimePoint = std::chrono::high_resolution_clock::now();

		// Rebuild fonts if needed for different size
		{
			OPTICK_EVENT("Rebuilding fonts");		
			RebuildFonts();
		}
				
		{
			OPTICK_EVENT("Polling events");
			if(!g_OSWindow->PollEvents()) {
				return false;
			}
		}

		if(m_bDrawDebugInfo) {
			std::string str;
			auto sb = Util::StringBuilder(&str);

			sb.Line("Frame time: {:5.2f}ms", m_LastFrameTimeMs);
			sb.Line("Opened window count: {}", m_WindowStack.size());
			sb.Line("Opened popup count: {}", m_PopupWindowsStack.size());
			sb.Line("Mouse pos global: {}", m_MousePosGlobal);
			sb.Line("Hovered window: {}", m_HoveredWindow ? m_HoveredWindow->GetDebugIDString() : "");
			sb.Line("Hovered widget: {}", m_HoveredWidget ? m_HoveredWidget->GetDebugIDString() : "");
			sb.Line("Capturing mouse widget: {}", m_CapturingWidget ? m_CapturingWidget->GetDebugIDString() : "");
			//sb.Line("MousePosLocal: {}", GetLocalHoveredPos());

			if(m_HoveredWindow) {
				sb.Line("{} Hit stack: ", m_HoveredWindow->GetDebugIDString());
				sb.PushIndent();
				PrintHitStack(sb);
			}
			m_WindowStack.back()->GetChild<Text>()->SetText(str);
		}
		
		//// Process tooltip
		//if(m_MouseState == MouseState::Default && m_Layers.Tooltip.Timer.IsReady()) {

		//	m_LastHitStack.Top().Widget->VisitParent([&](Widget* inWidget)->VisitResult {

		//		if(auto* tooltipSpawner = inWidget->As<TooltipSpawner>()) {
		//			m_Layers.Tooltip.Widget.reset(tooltipSpawner->Spawn());
		//			m_Layers.Tooltip.Widget->SetOrigin(m_LastMousePosGlobal);
		//			m_MouseState = MouseState::Tooltip;
		//			return VisitResultExit;
		//		}
		//		return VisitResultContinue;
		//	});
		//	m_Layers.Tooltip.Timer.Clear();
		//}

		// Draw views
		g_Renderer->ResetDrawLists();
		auto* frameDrawList = g_Renderer->GetFrameDrawList();
		frameDrawList->PushFont(m_Theme->GetDefaultFont(), g_DefaultFontSize);

		{
			OPTICK_EVENT("Drawing UI");

			ForEachWindow([&](Window* inWindow) {
				if(inWindow->IsVisible()) {
					DrawWindow(inWindow, frameDrawList, m_Theme.get());
				}
				return true;
			});
		}
		const auto frameEndTimePoint = std::chrono::high_resolution_clock::now();

		++m_FrameNum;
		m_LastFrameTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTimePoint - frameStartTimePoint).count() / 1000.f;
		
		g_Renderer->RenderFrame(true);
		// Render UI async
		/*{
			if(m_RenderingJobEventRef && !m_RenderingJobEventRef.IsSignalled()) {
				JobSystem::ThisFiber::WaitForEvent(m_RenderingJobEventRef);
				m_RenderingJobEventRef.Reset();
			}
			JobSystem::Builder jb;

			auto renderingJob = [](JobSystem::JobContext&) {
				OPTICK_EVENT("Rendering UI vsync");
				g_Renderer->RenderFrame(true);
			};
			jb.PushBack(renderingJob);
			m_RenderingJobEventRef = jb.PushFence();
			jb.Kick();
		}*/
		
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

		if(auto* spawnPopupIntent = inEvent->Cast<SpawnPopupIntent>()) {
			auto popup = spawnPopupIntent->Spawner->OnSpawn(m_MousePosGlobal);
			Assertm(popup->HasAnyWindowFlags(WindowFlags::Popup), "The spawned window should have WindowFlags::Popup flag set");
			popup->OnParented(this);
			m_PopupWindowsStack.emplace_back(PopupWindow{std::move(popup), spawnPopupIntent->Spawner});

			// Because we open a new window on top of other windows, update hovering state
			if(m_PopupWindowsStack.size() == 1) {
				DispatchMouseMoveEvent(m_MousePosGlobal);
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

	// If Visitor returns false, exit
	void	ForEachWindow(
		const std::function<bool(Window*)>& inVisitor, 
		bool bForward = true,
		bool bFloatWindows = true,
		bool bPopupWindows = true)
	{
		if(!bForward) {
			if(bPopupWindows) {
				for(auto it = m_PopupWindowsStack.rbegin(); it != m_PopupWindowsStack.rend(); ++it) {					
					if(!inVisitor(it->Window.get())) {
						return;
					}
				}
			}
			if(bFloatWindows) {
				for(auto it = m_WindowStack.rbegin(); it != m_WindowStack.rend(); ++it) {
					if(!inVisitor(it->get())) {
						return;
					}
				}
			}
		} else {
			if(bFloatWindows) {
				for(auto it = m_WindowStack.begin(); it != m_WindowStack.end(); ++it) {
					if(!inVisitor(it->get())) {
						return;
					}
				}
			}
			if(bPopupWindows) {
				for(auto it = m_PopupWindowsStack.begin(); it != m_PopupWindowsStack.end(); ++it) {
					if(!inVisitor(it->Window.get())) {
						return;
					}
				}
			}			
		}
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

	Point					m_MousePosGlobal;
	Window*					m_HoveredWindow = nullptr;
	
	// Number of buttons currently held
	u8						m_MouseButtonHeldNum = 0;
	// We allow only one button to be pressed simultaniously
	MouseButton				m_PressedMouseButton = MouseButton::None;
	MouseButtonMask			m_MouseButtonsPressedBitField = MouseButtonMask::None;
	// Position of the mouse cursor when button has been pressed
	Point					m_MousePosOnCaptureGlobal;

	HitStack				m_LastHitStack;
	Widget*					m_HoveredWidget = nullptr;
	Widget*					m_CapturingWidget = nullptr;

	bool					m_ModifiersState[(int)KeyModifiers::Count] = {false};

	// Stack of windows, bottom are background windows and top are overlay windows
	// Other windows in the middle
	std::list<std::unique_ptr<Window>>		
							m_WindowStack;

	struct PopupWindow {
		std::unique_ptr<Window> Window;
		PopupSpawner*			Spawner = nullptr;
	};
	std::list<PopupWindow>	m_PopupWindowsStack;


	struct TooltipState {
		std::unique_ptr<LayoutWidget>	Widget;
		Timer							Timer;
	};
	TooltipState			m_Tooltip;

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