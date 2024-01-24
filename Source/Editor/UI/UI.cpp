#include "UI.h"
#include "Widget.h"
#include "Runtime/Platform/Window.h"
#include "Runtime/System/Renderer/UIRenderer.h"

#include <stack>

#define GAVAUI_BEGIN namespace UI {
#define GAVAUI_END }

GAVAUI_BEGIN

class RootWindow;

constexpr u8	g_DefaultFontSize = 13;

RootWindow*		g_RootWindow = nullptr;
INativeWindow*	g_OSWindow = nullptr;
Theme*			g_DefaultTheme = nullptr;
Renderer*		g_Renderer = nullptr;

// Draw hitstack and mouse pos
bool			g_bDrawDebugInfo = false;

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
		, m_Indent(0)
	{}

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




/*
* Root background area of the OS window
*/
class RootWindow: public SingleChildContainer {
	DEFINE_CLASS_META(RootWindow, SingleChildContainer)
public:

	enum class StateFlags {
		Default,
		MouseButtonHold,
	};
	DEFINE_ENUM_FLAGS_OPERATORS_FRIEND(StateFlags)

public:

	RootWindow() {
		Super::SetAxisMode(AxisModeExpand);
	}
		
	// Ignore all events
	bool	OnEvent(IEvent* inEvent) override {

		if(auto event = inEvent->As<ChildEvent>()) {
			return Super::OnEvent(inEvent);
		}
		return false;
	}

	void	HandleMouseMoveEvent(float x, float y) {

		if(m_HeldMouseButton != MouseButtonEnum::None) {

			if(m_CapturesMouse) {
				auto mousePosGlobal = float2(x, y);
				auto mousePosLocal = mousePosGlobal;

				// Convert global to local using hit test data
				const auto parentHitData = m_LastHitTest.FindHitDataFor(m_CapturesMouse->GetParent());

				if(parentHitData.has_value()) {
					mousePosLocal = parentHitData->HitPosLocal + (mousePosGlobal - m_LastHitTest.HitPosGlobal);
				}

				MouseDragEvent dragEvent;
				dragEvent.CursorPosGlobalSpace = {x, y};				
				dragEvent.CursorPosLocalSpace = mousePosLocal;
				dragEvent.CursorDelta = dragEvent.CursorPosGlobalSpace - m_LastMousePosGlobal;
				m_CapturesMouse->OnEvent(&dragEvent);
			}

		} else {
			HitTestEvent hitTest;
			hitTest.HitPosGlobal = {x, y};
			Super::DispatchToChildren(&hitTest);

			Widget* hovered = nullptr;

			// Notify last widget in the stack that it's hovered
			if(!hitTest.HitStack.empty()) {
				//LOGF("Last hit widget class: {}", hitTest.HitStack.back().Widget->_GetClassName());
				HoverEvent hoverEvent{true};

				for(auto* last = hitTest.HitStack.back().Widget; last; last = last->GetParent()) {
					auto bHandled = last->OnEvent(&hoverEvent);

					if(bHandled) {
						hovered = last;
						break;
					}
				}
			}

			// Notify previous last widget in the stack that it's no longer hovered
			if(m_LastHovered && hovered != m_LastHovered) {
				HoverEvent hoverEvent{false};
				m_LastHovered->OnEvent(&hoverEvent);
			}
			m_LastHovered = hovered;
			m_LastHitTest = hitTest;
		}
		m_LastMousePosGlobal = {x, y};
	}

	void	HandleMouseButtonEvent(KeyCode inButton, bool bPressed) {

		if(bPressed && m_HeldMouseButton == MouseButtonEnum::None) {
			m_HeldMouseButton = (MouseButtonEnum)inButton;

			if(!m_LastHitTest.HitStack.empty()) {
				MouseButtonEvent event;
				event.bButtonPressed = bPressed;
				event.Button = (MouseButtonEnum)inButton;

				for(auto* last = m_LastHitTest.GetLastWidget(); last; last = last->GetParent()) {
					auto bHandled = last->OnEvent(&event);

					if(bHandled) {
						m_CapturesMouse = last;
						break;
					}
				}
			}

		} else if(!bPressed && (MouseButtonEnum)inButton == m_HeldMouseButton) {
			m_HeldMouseButton = MouseButtonEnum::None;
			m_CapturesMouse = nullptr;
		}
	}

	void	HandleKeyInputEvent(KeyCode inButton, bool bPressed) {
		if(inButton == KeyCode::KEY_P && bPressed) {
			LogWidgetTree();
		} else if(inButton == KeyCode::KEY_D && bPressed) {
			g_bDrawDebugInfo = !g_bDrawDebugInfo;
		}
	}

	void	HandleMouseScrollEvent(float inScroll) {}

	void	HandleNativeWindowResizeEvent(float2 inWindowSize) { 
		// Ignore minimized state for now
		if(inWindowSize == float2()) return;		

		Super::SetSize(inWindowSize); 
		g_Renderer->ResizeFramebuffers(inWindowSize);

		ParentEvent layoutEvent;
		layoutEvent.Constraints = inWindowSize;
		layoutEvent.bAxisChanged = {true, true};
		Super::DispatchToChildren(&layoutEvent);
	}

	void	HandleKeyCharInputEvent(wchar_t inChar) {}

	void	Draw(DrawList* inDrawList) {
		DrawEvent drawEvent;
		drawEvent.DrawList = inDrawList;
		drawEvent.ParentOriginGlobal = Point(0.f, 0.f);
		Super::DispatchToChildren(&drawEvent);
	}

	// Draws debug hit stack
	void	DrawHitStack(VerticalTextDrawer& inDrawer) {
		Debug::PropertyArchive ar;

		for(auto& hitData : m_LastHitTest.HitStack) {
			ar.PushObject(hitData.Widget->_GetClassName(), hitData.Widget, nullptr);
			hitData.Widget->DebugSerialize(ar);
		}

		for(auto& object : ar.m_RootObjects) {
			inDrawer.DrawText(object->m_ClassName);
			inDrawer.PushIndent();

			for(auto& property : object->m_Properties) {
				inDrawer.DrawTextF("{}: {}", property.Name, property.Value);
			}
			inDrawer.PopIndent();
		}
	}

	void	DrawDebug(DrawList* inDrawList) {
		VerticalTextDrawer drawer(inDrawList, Point(), Color("#dddddd"));
		drawer.DrawTextF("MousePosGlobal: {}", m_LastMousePosGlobal);
		drawer.DrawTextF("MousePosLocal: {}", GetLocalHoveredPos());
		drawer.DrawText("HitStack: ");
		drawer.PushIndent();
		DrawHitStack(drawer);
	}

	Point	GetLocalHoveredPos() {
		if(m_LastHovered) {
			for(auto& obj : m_LastHitTest.HitStack) {
				if(obj.Widget == m_LastHovered) {
					return obj.HitPosLocal;
				}
			}
		}
		return m_LastMousePosGlobal;
	}

	void	LogWidgetTree() {
		Debug::PropertyArchive archive;
		DebugLogEvent onDebugLog;
		onDebugLog.Archive = &archive;

		DispatchToChildren(&onDebugLog);

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

	bool	HasAnyFlags(StateFlags inFlags) const { return m_StateFlags & inFlags; }
	void	SetFlags(StateFlags inFlags) { m_StateFlags |= inFlags; }
	void	ClearFlags(StateFlags inFlags) { m_StateFlags &= ~inFlags; }

private:
	Point			m_LastMousePosGlobal;
	Widget*			m_LastHovered = nullptr;
	Widget*			m_CapturesMouse = nullptr;
	HitTestEvent	m_LastHitTest;
	StateFlags		m_StateFlags = StateFlags::Default;

	// Currently held mouse button
	MouseButtonEnum	m_HeldMouseButton = MouseButtonEnum::None;

};

void Init(std::string_view inWindowTitle, u32 inWidth, u32 inHeight) {

	// Take window position and size from command line
	// Take window parameters from config file

	g_RootWindow = new RootWindow();
	g_OSWindow = INativeWindow::createWindow(inWindowTitle, inWidth, inHeight);

	g_OSWindow->SetOnCursorMoveCallback([](float x, float y) { g_RootWindow->HandleMouseMoveEvent(x, y); });
	g_OSWindow->SetOnMouseButtonCallback([](KeyCode inButton, bool bPressed) { g_RootWindow->HandleMouseButtonEvent(inButton, bPressed); });
	g_OSWindow->SetOnMouseScrollCallback([](float inScroll) { g_RootWindow->HandleMouseScrollEvent(inScroll); });
	g_OSWindow->SetOnWindowResizedCallback([](float2 inWindowSize) { g_RootWindow->HandleNativeWindowResizeEvent(inWindowSize); });
	g_OSWindow->SetOnKeyboardButtonCallback([](KeyCode inButton, bool bPressed) { g_RootWindow->HandleKeyInputEvent(inButton, bPressed); });
	g_OSWindow->SetOnCharInputCallback([](wchar_t inCharacter) { g_RootWindow->HandleKeyCharInputEvent(inCharacter); });

	g_Renderer = CreateRendererDX12();
	g_Renderer->Init(g_OSWindow);

	//m_WorkingDirectoryFilename = Path(argv[0]).parent_path();
	//LOGF("Info: Application has been initialized. Window size: {}x{}, Working directory: {}.", inWidth, inHeight, m_WorkingDirectoryFilename);
	g_DefaultTheme = new Theme();

	if(!g_DefaultTheme->Font) {
		auto* font = CreateFontInternal();
		g_DefaultTheme->Font.reset(font);
		font->RasterizeFace(g_DefaultFontSize);
	}

	if(g_DefaultTheme->Font->NeedsRebuild()) {
		Image fontImageAtlas;
		g_DefaultTheme->Font->Build(&fontImageAtlas);
		auto oldTexture = (TextureHandle)g_DefaultTheme->Font->GetAtlasTexture();
		TextureHandle fontTexture = g_Renderer->CreateTexture(fontImageAtlas);

		g_DefaultTheme->Font->SetAtlasTexture(fontTexture);
		g_Renderer->DeleteTexture(oldTexture);
	}
}

bool Tick() {
		
	// Update layout of new widgets
	// Process input events
	// Update layout of changed widgets
	// Draw
	if(g_DefaultTheme->Font->NeedsRebuild()) {
		Image fontImageAtlas;
		g_DefaultTheme->Font->Build(&fontImageAtlas);
		auto oldTexture = (TextureHandle)g_DefaultTheme->Font->GetAtlasTexture();
		TextureHandle fontTexture = g_Renderer->CreateTexture(fontImageAtlas);

		g_DefaultTheme->Font->SetAtlasTexture(fontTexture);
		g_Renderer->DeleteTexture(oldTexture);
	}

	if(!g_OSWindow->PollEvents()) {
		return false;
	}

	// Draw views
	g_Renderer->ResetDrawLists();
	auto& frameDrawList = *g_Renderer->GetFrameDrawList();
	frameDrawList.PushFont(g_DefaultTheme->Font.get(), g_DefaultFontSize);

	g_RootWindow->Draw(&frameDrawList);

	if(g_bDrawDebugInfo) {
		g_RootWindow->DrawDebug(&frameDrawList);
	}	
	g_Renderer->RenderFrame(true);

	//DrawEventData drawContext;
	//drawContext.DrawList = &frameDrawList;
	//drawContext.Style = *m_SelectedStyle;
	//drawContext.CursorPosViewportSpace = m_LastMousePos;

	//for(auto* window : m_FloatWindowStack) {
	//	// If window has parent, parent will be responsible for drawing
	//	if(!window->IsDocked() && window->IsVisible()) {
	//		window->OnDraw(drawContext);
	//	}
	//}
}

UI::Widget* UI::GetRoot() {
	return g_RootWindow;
}

UI::Theme* UI::GetDefaultTheme() {
	return g_DefaultTheme;
}

GAVAUI_END