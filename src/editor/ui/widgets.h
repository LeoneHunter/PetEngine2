#pragma once
#include "framework.h"

namespace UI {

class Style;
class TextStyle;
class BoxStyle;
class LayoutStyle;
class Flexbox;
class Widget;
class PopupSpawner;
class Application;

// Common state enums
// Used by button like widgets
struct State {
	static inline StringID Normal{"Normal"};
	static inline StringID Hovered{"Hovered"};
	static inline StringID Pressed{"Pressed"};
};



/*
 * Displays not editable text
 */
class Text: public LayoutWidget {
	WIDGET_CLASS(Text, LayoutWidget)
public:
	static const inline StringID defaultStyleName{"Text"};

	static Text* New(const std::string& inText, StringID inStyle = defaultStyleName) {
		return new Text(inText, inStyle);
	}

	void SetText(const std::string& inText);
	bool OnEvent(IEvent* inEvent) override;

protected:
	Text(const std::string& inText, StringID inStyle);

private:
	const StyleClass* m_Style;
	std::string       m_Text;
};



enum class SizeMode {
	Fixed,
	ShrinkWrap,
	Expand
};

/*
 * A layout widget that contains a single child
 * Draws a simple rect
 */
class Container: public SingleChildLayoutWidget {
	WIDGET_CLASS(Container, SingleChildLayoutWidget)
public:
	static Container* New(SizeMode inSizeMode,
						  bool     bClip,
						  StringID inStyleName,
						  Widget*  inChild) {
		auto* out = new Container(inSizeMode, bClip, inStyleName);
		out->SetChild(inChild);
		return out;
	}

	static Container* NewFixed(float2 inSize, bool bClip, StringID inStyleName, Widget* inChild) {
		auto out = new Container(SizeMode::Fixed, bClip, inStyleName);
		out->SetSize(inSize);
		out->SetChild(inChild);
		return out;
	}

	bool OnEvent(IEvent* inEvent) override {
		if(auto* layoutEvent = inEvent->Cast<ParentLayoutEvent>()) {
			if(HandleParentLayoutEvent(layoutEvent)) {
				DispatchLayoutToChildren();
			}
			return true;
		}

		if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
			if(m_SizeMode == SizeMode::ShrinkWrap && HandleChildEvent(event)) {
				DispatchLayoutToParent();
			}
			return true;
		}

		if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
			drawEvent->drawList->PushBox(GetRect(), m_Style->FindOrDefault<BoxStyle>(m_BoxStyleName));
			if(m_bClip)
				drawEvent->drawList->PushClipRect(GetRect());

			DispatchDrawToChildren(drawEvent);
			if(m_bClip)
				drawEvent->drawList->PopClipRect();
			return true;
		}
		return Super::OnEvent(inEvent);
	}

	// Sets style selector for box style used for drawing
	void SetBoxStyleName(StringID inName) {
		m_BoxStyleName = inName;
	}

private:
	Container(SizeMode inSizeMode,
			  bool     bClip,
			  StringID inStyleName)
		: Super(inStyleName, axisModeShrink)
		, m_SizeMode(inSizeMode)
		, m_bClip(bClip)
		, m_Style(Application::Get()->GetTheme()->Find(inStyleName)) {
		if(inSizeMode == SizeMode::Fixed) 
			SetFloatLayout(true);
	}

private:
	SizeMode m_SizeMode;
	bool     m_bClip;
	// Cached style from current Theme
	const StyleClass* m_Style;
	StringID          m_BoxStyleName;
};



class Button;

class ButtonEvent: public IEvent {
	EVENT_CLASS(ButtonEvent, IEvent);

public:
	ButtonEvent(Button* inSource, MouseButton inBtn, bool bPressed)
		: source(inSource), button(inBtn), bPressed(bPressed) {}

	EventCategory GetCategory() const override { return EventCategory::Callback; }

	Button*     source;
	MouseButton button;
	bool        bPressed;
};

using ButtonEventFunc = std::function<void(ButtonEvent*)>;

/*
 * Simple clickable button with flat style
 */
class Button: public StatefulWidget {
	WIDGET_CLASS(Button, StatefulWidget)
public:
	static Button* New(StringID inStyleName, const ButtonEventFunc& inEventCallback, Widget* inChild) {
		auto* btn = new Button(inEventCallback);
		btn->SetChild(Container::New(SizeMode::ShrinkWrap, false, inStyleName, inChild));
		return btn;
	}

	bool OnEvent(IEvent* inEvent) override {
		if(auto* event = inEvent->Cast<HoverEvent>()) {
			// Change state and style
			if(event->bHoverEnter) {
				m_State = State::Hovered;
				FindChildOfClass<Container>()->SetBoxStyleName(State::Hovered);

			} else if(event->bHoverLeave) {
				m_State = State::Normal;
				FindChildOfClass<Container>()->SetBoxStyleName(State::Normal);
			}
			return true;
		}
		if(auto* event = inEvent->Cast<MouseButtonEvent>()) {
			if(event->button == MouseButton::ButtonLeft && !event->bHandled) {
				
				if(event->bButtonPressed) {
					m_State = State::Pressed;
					FindChildOfClass<Container>()->SetBoxStyleName(State::Pressed);

				} else {
					auto* container = FindChildOfClass<Container>(); 

					if(container->GetRect().Contains(event->mousePosLocal)) {
						m_State = State::Hovered;
						container->SetBoxStyleName(State::Hovered);
					} else {
						m_State = State::Normal;
						container->SetBoxStyleName(State::Normal);
					}
				}
				if(m_EventCallback) {
					ButtonEvent btnEvent(this, event->button, event->bButtonPressed);
					m_EventCallback(&btnEvent);
				}
				return true;
			}
			return false;
		}
		return Super::OnEvent(inEvent);
	}

	void DebugSerialize(PropertyArchive& inArchive) override {
		Super::DebugSerialize(inArchive);
		inArchive.PushProperty("ButtonState", *m_State);
	}

protected:
	Button(const ButtonEventFunc& inEventCallback)
		: m_State(State::Normal)
		, m_EventCallback(inEventCallback) {}

private:
	StringID        m_State;
	ButtonEventFunc m_EventCallback;
};



class WindowBuilder;

enum class WindowFlags {
	None        = 0,
	AlwaysOnTop = 0x1,
	Overlay     = 0x2,
	Background  = 0x4,
	ShrinkToFit = 0x8,
	Popup       = 0x10,
};
DEFINE_ENUM_FLAGS_OPERATORS(WindowFlags)

struct WindowConfig {
	static const inline StringID defaultStyleName = "Window";

	std::string id;
	StringID    style = defaultStyleName;
	WindowFlags flags = WindowFlags::None;
	Point       pos;
	float2      size;
	// Custom title bar widget
	mutable std::unique_ptr<Widget> titleBar;
	// Custom header added to the rop of the content area
	mutable std::unique_ptr<Widget> header;
	// Custom footer added to the botton of the content area
	mutable std::unique_ptr<Widget> footer;
	// Custom content widget that contains actual children
	mutable std::unique_ptr<Widget> content;
};

/*
 * A container that could be used as a root widget
 * E.g. a background root widget resized to the underlying os window
 * Can be customized to add resizing borders, title bar, menu bar (header), status bar (footer), close button
 */
class Window: public StatefulWidget {
	WIDGET_CLASS(Window, StatefulWidget)
public:
	static WindowBuilder Build();
	static Window*       New(const WindowConfig& inConfig) { return new Window(inConfig); }

	bool OnEvent(IEvent* inEvent) override;
	bool HasAnyWindowFlags(WindowFlags inFlags) const { return m_Flags & inFlags; }

protected:
	Window(const WindowConfig& inConfig);

private:
	const StyleClass* m_Style;
	WindowFlags       m_Flags;
};

/*
 * Helper to configure a window procedurally
 */
class WindowBuilder {
public:
	WindowBuilder() {}

	WindowBuilder& DefaultFloat() { return *this; }
	WindowBuilder& ID(const std::string& inID) { config.id = inID; return *this; }
	WindowBuilder& StyleClass(const std::string& inStyle) { config.style = inStyle; return *this; }
	WindowBuilder& Position(Point inPos) { config.pos = inPos; return *this; }
	WindowBuilder& Position(float inX, float inY) { config.pos = {inX, inY}; return *this; }
	WindowBuilder& Size(float2 inSize) { config.size = inSize; return *this; }
	WindowBuilder& Size(float inX, float inY) { config.size = {inX, inY}; return *this; }

	Window* New() { return Window::New(config); }

private:
	WindowConfig config;
};

inline WindowBuilder Window::Build() { return WindowBuilder(); }



// /*-------------------------------------------------------------------------------------------------*/
// //										CENTERED
// /*-------------------------------------------------------------------------------------------------*/
// 	/*
// 	* Takes all allocated space from the parent
// 	* and places the child inside in the center
// 	*/
// 	class Centered: public SingleChildContainer {
// 		WIDGET_CLASS(Centered, SingleChildContainer)
// 	public:
// 		Centered(Widget* inParent, WidgetSlot inSlot = defaultWidgetSlot);
// 		void OnParented(Widget* inParent) override;
// 		bool OnEvent(IEvent* inEvent) override;
// 	};


// /*-------------------------------------------------------------------------------------------------*/
// //										FLEXIBLE
// /*-------------------------------------------------------------------------------------------------*/

// 	/*
// 	* TODO
// 	*	Handle size update
// 	*/
// 	/*class Flexible: public SingleChildContainer {
// 		DEFINE_CLASS_META(Flexible, SingleChildContainer)
// 	public:

// 		Flexible(Widget* inParent, float inFlexFactor, const std::string& inID = {});

// 		void  DebugSerialize(Debug::PropertyArchive& inArchive) override {
// 			Super::DebugSerialize(inArchive);
// 			inArchive.PushProperty("FlexFactor", m_FlexFactor);
// 		}

// 		float GetFlexFactor() const { return m_FlexFactor; }

// 	private:
// 		float m_FlexFactor;
// 	};*/



/*-------------------------------------------------------------------------------------------------*/
//										FLEXBOX
/*-------------------------------------------------------------------------------------------------*/

enum class ContentDirection {
	Column,
	Row
};

enum class JustifyContent {
	Start,
	End,
	Center,
	SpaceBetween,
	SpaceAround
};

enum class AlignContent {
	Start,
	End,
	Center
};

enum class OverflowPolicy {
	Clip,        // Just clip without scrolling
	Wrap,        // Wrap children on another line
	ShrinkWrap,  // Do not decrease containter size below content size
				 // Could be used with Scrolled widget to scroll content on overflow
};

struct FlexboxConfig {
	static const inline StringID defaultStyleName = "Flexbox";

	std::string 	     id;
	StringID    		 style = defaultStyleName;
	UI::ContentDirection direction = ContentDirection::Row;
	UI::JustifyContent   justifyContent = JustifyContent::Start;
	UI::AlignContent     alignment = AlignContent::Start;
	UI::OverflowPolicy   overflowPolicy = OverflowPolicy::Clip;
	bool                 expandMainAxis = true;
	bool                 expandCrossAxis = false;
	std::vector<Widget*> children;
};

/*
 * Vertical or horizontal flex container
 * Similar to Flutter and CSS
 * Default size behavior is shrink to fit children but can be set
 */
class Flexbox: public MultiChildLayoutWidget {
	WIDGET_CLASS(Flexbox, MultiChildLayoutWidget)
public:
	Flexbox(const FlexboxConfig& inConfig);

	bool OnEvent(IEvent* inEvent) override;
	void DebugSerialize(PropertyArchive& inArchive) override;

	ContentDirection GetDirection() const { return m_Direction; }

private:
	void UpdateLayout();

private:
	ContentDirection m_Direction;
	JustifyContent   m_JustifyContent;
	AlignContent     m_Alignment;
	OverflowPolicy   m_OverflowPolicy;
};

DEFINE_ENUM_TOSTRING_2(ContentDirection, Column, Row)
DEFINE_ENUM_TOSTRING_5(JustifyContent, Start, End, Center, SpaceBetween, SpaceAround)
DEFINE_ENUM_TOSTRING_3(AlignContent, Start, End, Center)
DEFINE_ENUM_TOSTRING_2(OverflowPolicy, Clip, Wrap)





// struct FlexboxBuilder: public WidgetBuilder<FlexboxBuilder> {
// 	// Direction of the main axis
// 	FlexboxBuilder& Direction(ContentDirection inDirection) {
// 		m_Direction = inDirection;
// 		return *this;
// 	}
// 	FlexboxBuilder& DirectionRow() {
// 		m_Direction = ContentDirection::Row;
// 		return *this;
// 	}
// 	FlexboxBuilder& DirectionColumn() {
// 		m_Direction = ContentDirection::Column;
// 		return *this;
// 	}
// 	// Distribution of children on the main axis
// 	FlexboxBuilder& JustifyContent(UI::JustifyContent inJustify) {
// 		m_JustifyContent = inJustify;
// 		return *this;
// 	}
// 	// Alignment on the cross axis
// 	FlexboxBuilder& Alignment(AlignContent inAlignment) {
// 		m_Alignment = inAlignment;
// 		return *this;
// 	}
// 	// What to do when children don't fit into container
// 	FlexboxBuilder& OverflowPolicy(OverflowPolicy inPolicy) {
// 		m_OverflowPolicy = inPolicy;
// 		return *this;
// 	}
// 	FlexboxBuilder& ExpandMainAxis(bool bExpand) {
// 		m_ExpandMainAxis = bExpand;
// 		return *this;
// 	}
// 	FlexboxBuilder& ExpandCrossAxis(bool bExpand) {
// 		m_ExpandCrossAxis = bExpand;
// 		return *this;
// 	}
// 	FlexboxBuilder& Expand() {
// 		m_ExpandCrossAxis = true;
// 		m_ExpandMainAxis  = true;
// 		return *this;
// 	}

// 	// Finalizes creation
// 	Flexbox* Create(Widget* inParent, WidgetSlot inSlot = defaultWidgetSlot);

// public:
// 	FlexboxBuilder()
// 		: m_Direction(ContentDirection::Row)
// 		, m_JustifyContent(JustifyContent::Start)
// 		, m_Alignment(AlignContent::Start)
// 		, m_OverflowPolicy(OverflowPolicy::Clip)
// 		, m_ExpandMainAxis(true)
// 		, m_ExpandCrossAxis(false) {
// 		StyleClass("Container");
// 	}

// 	friend class Flexbox;

// public:
// 	UI::ContentDirection m_Direction;
// 	UI::JustifyContent   m_JustifyContent;
// 	UI::AlignContent     m_Alignment;
// 	UI::OverflowPolicy   m_OverflowPolicy;
// 	bool                 m_ExpandMainAxis;
// 	bool                 m_ExpandCrossAxis;
// };
// /*-------------------------------------------------------------------------------------------------*/
// //										GUIDELINE
// /*-------------------------------------------------------------------------------------------------*/

// 	/*
// 	* A line that can be moved by the user
// 	* Calls a callback when dragged
// 	* Moving should be handled externally
// 	*/
// 	class Guideline: public LayoutWidget {
// 		WIDGET_CLASS(Guideline, LayoutWidget)
// 	public:
// 		// @param float Delta - a dragged amount in one axis
// 		using OnDraggedFunc = VoidFunction<MouseDragEvent*>;

// 		Guideline(bool bIsVertical, OnDraggedFunc inCallback, Widget* inParent, WidgetSlot inSlot = defaultWidgetSlot);
// 		void SetCallback(OnDraggedFunc inCallback) { m_Callback = inCallback; }
// 		bool OnEvent(IEvent* inEvent) override;

// 	public:
// 		void DebugSerialize(Debug::PropertyArchive& inArchive) override;

// 	private:
// 		Axis			m_MainAxis;
// 		StringID		m_State;
// 		OnDraggedFunc	m_Callback;
// 	};


// /*-------------------------------------------------------------------------------------------------*/
// //										SPLITBOX
// /*-------------------------------------------------------------------------------------------------*/

// 	/*
// 	* A container that has 3 children: First, Second and Separator
// 	* The user can drag the Separator to resize First and Second children
// 	* It's expanded on both axes
// 	* Children must also be expanded on both axes
// 	*/
// 	class SplitBox: public Container {
// 		DEFINE_CLASS_META(SplitBox, Container)
// 	public:

// 		constexpr static inline WidgetSlot FirstSlot{1};
// 		constexpr static inline WidgetSlot SecondSlot{2};

// 		SplitBox(bool bHorizontal, Widget* inParent, WidgetSlot inSlot = defaultWidgetSlot);

// 		bool OnEvent(IEvent* inEvent) override;
// 		bool DispatchToChildren(IEvent* inEvent) override;
// 		void DebugSerialize(Debug::PropertyArchive& inArchive) override;
// 		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive) override;

// 		void OnSeparatorDragged(MouseDragEvent* inDragEvent);
// 		void SetSplitRatio(float inRatio);

// 	private:

// 		void UpdateLayout();

// 	private:
// 		std::unique_ptr<Guideline>		m_First;
// 		std::unique_ptr<Guideline>		m_Second;
// 		std::unique_ptr<Guideline>		m_Separator;
// 		Axis							m_MainAxis;
// 		float							m_SplitRatio;
// 	};

using TooltipBuilderFunc = std::function<Widget*()>;

/*
 * Builds a simple text tooltip
 */
class Tooltip {
public:
	static Widget* NewText(const std::string& inText) {
		return Container::New(SizeMode::ShrinkWrap, true, "Tooltip", Text::New(inText));
	}
};


/*
 * Opens a tooltip when hovered
 */
class TooltipPortal: public SingleChildWidget {
	WIDGET_CLASS(TooltipPortal, SingleChildWidget)
public:
	static TooltipPortal* New(const TooltipBuilderFunc& inBuilder, Widget* inChild) {
		auto* out      = new TooltipPortal();
		out->m_Builder = inBuilder;
		out->SetChild(inChild);
		return out;
	}

	bool OnEvent(IEvent* inEvent) override;

	void DebugSerialize(PropertyArchive& ar) override {
		Super::DebugSerialize(ar);
		ar.PushProperty("Timer", (uintptr_t)sharedState.timerHandle);
		ar.PushProperty("Tooltip", sharedState.widget ? sharedState.widget->GetDebugID() : "nullptr");
		ar.PushProperty("bDisabled", sharedState.bDisabled);
	}

protected:
	TooltipPortal() = default;

private:
	void CloseTooltip();

private:
	// We use static shared state because there's no point in 
	//     having multipler tooltips visible at once
	struct State {
		TimerHandle timerHandle{};
		Widget* 	widget = nullptr;
		bool		bDisabled = false;
	};
	static inline State sharedState{};

	TooltipBuilderFunc m_Builder;
};



// /*-------------------------------------------------------------------------------------------------*/
// //										POPUP
// /*-------------------------------------------------------------------------------------------------*/
// 	class PopupWindow;
// 	class PopupSpawner;
// 	struct PopupBuilder;

// 	struct PopupSpawnContext {
// 		Point			MousePosGlobal;
// 		float2			ViewportSize;
// 		PopupSpawner*	Spawner;
// 	};


// 	/*
// 	* A popup window created by a PopupSpawner and owned by Application but can
// 	* be closed by the spawner
// 	* Children should be wrapped in a PopupItem in order to send notifications to this window
// 	*/
// 	class PopupWindow: public Window {
// 		WIDGET_CLASS(PopupWindow, Window)
// 	public:

// 		PopupWindow(const PopupBuilder& inBuilder);
// 		~PopupWindow();
// 		bool OnEvent(IEvent* inEvent) override;

// 		// Called by a child spawner when a child widget is hovered
// 		void					OnItemHovered();
// 		void					OnItemPressed();
// 		// Creates a popup and parents it to itself
// 		WeakPtr<PopupWindow>	OpenPopup(PopupSpawner* inSpawner);

// 		void SetSpawner(PopupSpawner* inSpawner) { m_Spawner = inSpawner; }

// 	private:
// 		PopupSpawner*					m_Spawner;
// 		std::unique_ptr<PopupWindow>	m_NextPopup;
// 		// A widget that spawned the next popup
// 		// Should be our child
// 		PopupSpawner*					m_NextPopupSpawner;
// 	};

// 	struct PopupBuilder: public WidgetBuilder<PopupBuilder> {

// 		PopupBuilder& Position(Point inPos) { m_Pos = inPos; return *this; }
// 		PopupBuilder& Position(float inX, float inY) { m_Pos = {inX, inY}; return *this; }
// 		PopupBuilder& Size(float2 inSize) { m_Size = inSize; return *this; }
// 		PopupBuilder& Size(float inX, float inY) { m_Size = {inX, inY}; return *this; }
// 		std::unique_ptr<PopupWindow> Create() { return std::make_unique<UI::PopupWindow>(*this); }

// 	public:
// 		PopupBuilder() { StyleClass("PopupWindow"); }

// 	public:
// 		Point		  m_Pos;
// 		float2		  m_Size;
// 	};


// 	// Called by the Application to create a PopupWindow
// 	using PopupSpawnFunc = std::function<std::unique_ptr<PopupWindow>(PopupSpawnContext)>;

// 	/*
// 	* Propagated up the widget stack until a PopupWindow or Application is found
// 	*/
// 	class PopupEvent final: public IEvent {
// 		EVENT_CLASS(PopupEvent, IEvent)
// 	public:
// 		EventCategory GetCategory() const override { return EventCategory::Intent; }

// 		static PopupEvent OpenPopup(PopupSpawner* inSpawner) {
// 			PopupEvent out;
// 			out.Type = Type::Open;
// 			out.Spawner = inSpawner;
// 			return out;
// 		}

// 		static PopupEvent ClosePopup(PopupWindow* inPopup) {
// 			PopupEvent out;
// 			out.Type = Type::Close;
// 			out.Popup = inPopup;
// 			return out;
// 		}

// 		static PopupEvent CloseAll() {
// 			PopupEvent out;
// 			out.Type = Type::CloseAll;
// 			return out;
// 		}

// 		enum class Type {
// 			Open,
// 			Close,
// 			CloseAll,
// 		};

// 		Type			Type = Type::Open;
// 		PopupSpawner*	Spawner = nullptr;
// 		PopupWindow*	Popup = nullptr;
// 	};


/*
 * Creates a user defined popup when clicked or hovered
 */
// class PopupSpawner: public SingleChildWidget {
// 	WIDGET_CLASS(PopupSpawner, SingleChildWidget)
// public:

// 	struct State {
// 		static inline StringID Normal{"Normal"};
// 		static inline StringID Opened{"Opened"};
// 	};

// 	enum class SpawnEventType {
// 		LeftMouseRelease,
// 		RightMouseRelease,
// 		Hover,
// 	};

// public:

// 	PopupSpawner(const PopupSpawnFunc&	inSpawner,
// 						SpawnEventType	inSpawnEvent = SpawnEventType::RightMouseRelease)
// 		: m_Spawner(inSpawner)
// 		, m_State(State::Normal)
// 		, m_SpawnEventType(inSpawnEvent)
// 	{}

// 	PopupSpawner(const PopupSpawnFunc&	inSpawner,
// 						Widget*			inParent,
// 						WidgetSlot		inSlot = defaultWidgetSlot,
// 						SpawnEventType	inSpawnEvent = SpawnEventType::RightMouseRelease)
// 		: PopupSpawner(inSpawner, inSpawnEvent)
// 	{
// 		if(inParent) inParent->Parent(this, inSlot);
// 	}

// 	bool					OnEvent(IEvent* inEvent) override {

// 		if(auto* event = inEvent->Cast<MouseButtonEvent>()) {

// 			if(m_SpawnEventType == SpawnEventType::RightMouseRelease && event->button == MouseButton::ButtonRight) {
// 				Super::OnEvent(inEvent);

// 				if(!event->bButtonPressed && m_State == State::Normal) {
// 					OpenPopup();
// 				}
// 				return true;
// 			}

// 			if(m_SpawnEventType == SpawnEventType::LeftMouseRelease && event->button == MouseButton::ButtonLeft) {
// 				Super::OnEvent(inEvent);

// 				if(!event->bButtonPressed && m_State == State::Normal) {
// 					OpenPopup();
// 				}
// 				return true;
// 			}
// 		}

// 		if(auto* hoverEvent = inEvent->Cast<HoverEvent>();
// 			hoverEvent && m_SpawnEventType == SpawnEventType::Hover)
// 		{
// 			const auto bHandledByChild = Super::OnEvent(inEvent);

// 			if(bHandledByChild && m_State == State::Normal) {
// 				OpenPopup();
// 			}
// 			return bHandledByChild;
// 		}

// 		return Super::OnEvent(inEvent);
// 	}

// 	// Called by Application to create a popup
// 	std::unique_ptr<PopupWindow> OnSpawn(Point inMousePosGlobal, float2 inWindowSize) {
// 		auto out = m_Spawner({inMousePosGlobal, inWindowSize, this});
// 		out->SetSpawner(this);
// 		m_Popup = out->GetWeak();
// 		m_State = State::Opened;
// 		return out;
// 	}

// 	void					OnPopupDestroyed() { m_State = State::Normal; }

// 	// Can be called by the user to create a popup
// 	void					OpenPopup() {
// 		if(!m_Popup) {
// 			auto msg = PopupEvent::OpenPopup(this);
// 			DispatchToParent(&msg);
// 		}
// 	}

// 	void					ClosePopup() {
// 		if(m_Popup) {
// 			auto msg = PopupEvent::ClosePopup(*m_Popup);
// 			DispatchToParent(&msg);
// 			m_State = State::Normal;
// 			Assert(!m_Popup);
// 			m_Popup = nullptr;
// 		}
// 	}

// 	bool					IsOpened() const { return m_State == State::Opened; }

// 	WeakPtr<PopupWindow>	GetPopupWeakPtr() { return m_Popup; }

// private:
// 	WeakPtr<PopupWindow>	m_Popup;
// 	SpawnEventType			m_SpawnEventType;
// 	PopupSpawnFunc			m_Spawner;
// 	StringID				m_State;
// };


// 	/*
// 	* When clicked closes parent popup
// 	* Can be used to place custom widgets into a popup window
// 	*/
// 	class PopupMenuItem: public Wrapper {
// 		WIDGET_CLASS(PopupMenuItem, Wrapper)
// 	public:

// 		PopupMenuItem() = default;
// 		bool OnEvent(IEvent* inEvent) override {

// 			if(auto* hoverEvent = inEvent->Cast<HoverEvent>()) {
// 				const auto bHandledByChild = Super::OnEvent(inEvent);

// 				if(bHandledByChild) {
// 					GetParent<PopupWindow>()->OnItemHovered();
// 				}
// 				return bHandledByChild;
// 			}

// 			if(auto* event = inEvent->Cast<MouseButtonEvent>()) {
// 				const auto bHandledByChild = Super::OnEvent(inEvent);

// 				if(bHandledByChild && !event->bButtonPressed) {
// 					GetParent<PopupWindow>()->OnItemPressed();
// 				}
// 				return bHandledByChild;
// 			}
// 			return Super::OnEvent(inEvent);
// 		}

// 	};

// 	struct ContextMenuBuilder;

// 	/*
// 	* A popup with buttons and icons
// 	*/
// 	class ContextMenu: public PopupWindow {
// 		WIDGET_CLASS(ContextMenu, PopupWindow)
// 	public:

// 		ContextMenu(const ContextMenuBuilder& inBuilder);
// 		void Parent(Widget* inWidget, WidgetSlot inSlot = defaultWidgetSlot) override;

// 	private:
// 		Flexbox* m_Container;
// 	};

// 	/*
// 	* A button that's added to the ContextMenu
// 	*/
// 	class ContextMenuItem: public PopupMenuItem {
// 		WIDGET_CLASS(ContextMenuItem, PopupMenuItem)
// 	public:

// 		ContextMenuItem(const std::string& inText, ContextMenu* inParent);
// 		bool OnEvent(IEvent* inEvent) override;
// 	};

// 	/*
// 	* A button that opens a submenu
// 	*/
// 	class SubMenuItem: public PopupSpawner {
// 		WIDGET_CLASS(SubMenuItem, PopupSpawner)
// 	public:

// 		SubMenuItem(const std::string& inText, const PopupSpawnFunc& inSpawner, ContextMenu* inParent);
// 		bool OnEvent(IEvent* inEvent) override;
// 	};


// 	struct ContextMenuBuilder: public WidgetBuilder<ContextMenuBuilder> {
// 		Point m_Pos;
// 		mutable	std::list<std::unique_ptr<Wrapper>> m_Children;

// 		ContextMenuBuilder& Position(Point inPos) { m_Pos = inPos; return *this; }
// 		ContextMenuBuilder& Position(float inX, float inY) { m_Pos = {inX, inY}; return *this; }
// 		ContextMenuBuilder& MenuItem(const std::string& inText) { m_Children.emplace_back(new ContextMenuItem(inText, nullptr)); return *this; }

// 		ContextMenuBuilder& SubMenu(const std::string& inText, const PopupSpawnFunc& inSpawner) {
// 			m_Children.emplace_back(new SubMenuItem(inText, inSpawner, nullptr)); return *this;
// 		}

// 		std::unique_ptr<ContextMenu> Create() { return std::make_unique<ContextMenu>(*this); };

// 		ContextMenuBuilder() { StyleClass("ContextMenu"); }
// 	};



// 	/*
// 	* A drawable rectangular box
// 	*/
// 	class Frame: public SingleChildContainer{
// 		WIDGET_CLASS(Frame, SingleChildContainer)
// 	public:

// 		Frame() {
// 			SetAxisMode(AxisModeShrink);
// 		}

// 		bool OnEvent(IEvent* inEvent) override {

// 			if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
// 				HandleChildEvent(event);
// 				NotifyParentOnSizeChanged();
// 				return true;
// 			}

// 			if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {

// 				if(m_Style) {
// 					drawEvent->DrawList->PushBox(GetRect(), m_Style->Find<BoxStyle>());
// 				}
// 				DispatchDrawToChildren(drawEvent);
// 				return true;
// 			}
// 			return Super::OnEvent(inEvent);
// 		}

// 		void SetStyle(const std::string& inStyleName) {
// 			m_Style = Application::Get()->GetTheme()->Find(inStyleName);
// 		}

// 	private:
// 		const StyleClass* m_Style = nullptr;
// 	};

}  // namespace UI