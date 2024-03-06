#pragma once
#include "Widget.h"

namespace UI {

	class Style;
	class TextStyle;
	class BoxStyle;
	class LayoutStyle;
	class Flexbox;	
	class Window;
	class PopupSpawner;
	class Application;

	// Common state enums
	// Used by button like widgets
	struct State {
		static inline StringID Normal{"Normal"};
		static inline StringID Hovered{"Hovered"};
		static inline StringID Pressed{"Pressed"};
	};

/*-------------------------------------------------------------------------------------------------*/
//										WINDOW
/*-------------------------------------------------------------------------------------------------*/

	enum class WindowFlags {
		None		= 0,
		AlwaysOnTop = 0x1,
		Overlay		= 0x2,
		Background	= 0x4,
		ShrinkToFit = 0x8,
		Popup		= 0x10,
	};
	DEFINE_ENUM_FLAGS_OPERATORS(WindowFlags)


	struct WindowBuilder: public WidgetBuilder<WindowBuilder> {

		WindowBuilder&	Flags(WindowFlags inFlags) { m_Flags = inFlags; return *this; }
		WindowBuilder&	Position(Point inPos) { m_Pos = inPos; return *this; }
		WindowBuilder&	Position(float inX, float inY) { m_Pos = {inX, inY}; return *this; }
		WindowBuilder&	Size(float2 inSize) { m_Size = inSize; return *this; }
		WindowBuilder&	Size(float inX, float inY) { m_Size = {inX, inY}; return *this; }
		// Finalizes creation
		Window*					Create(Application* inApp);
		std::unique_ptr<Window> Create();

	public:
		WindowBuilder(): m_Flags(WindowFlags::None) {
			StyleClass("Window");
		}
		friend class Window;

	public:
		WindowFlags  m_Flags;
		Point		 m_Pos;
		float2		 m_Size;
		Application* m_App = nullptr;
	};
	
	/*
	* Root parent container for widgets
	* An Application always have a background window with the size of the OS window
	*/
	class Window: public SingleChildContainer {
		WIDGET_CLASS(Window, SingleChildContainer)
	public:

		Window(Application*			inApp, 
			   const std::string&	inID, 
			   WindowFlags			inFlags, 
			   const std::string&	inStyleClassName = "Window");

		Window(const WindowBuilder& inBuilder);
		Point		TransformLocalToGlobal(Point inPosition) const;
		bool		OnEvent(IEvent* inEvent) override;
		LayoutInfo	GetLayoutInfo() const override;

		bool		HasAnyWindowFlags(WindowFlags inFlags) const { return m_Flags & inFlags; }

	private:
		const StyleClass*	m_Style;
		WindowFlags			m_Flags;		
	};



/*-------------------------------------------------------------------------------------------------*/
//										CENTERED
/*-------------------------------------------------------------------------------------------------*/
	/*
	* Takes all allocated space from the parent
	* and places the child inside in the center
	*/
	class Centered: public SingleChildContainer {
		WIDGET_CLASS(Centered, SingleChildContainer)
	public:
		Centered(Widget* inParent, WidgetSlot inSlot = DefaultWidgetSlot);
		void OnParented(Widget* inParent) override;
		bool OnEvent(IEvent* inEvent) override;
	};


/*-------------------------------------------------------------------------------------------------*/
//										FLEXIBLE
/*-------------------------------------------------------------------------------------------------*/

	/*
	* TODO
	*	Handle size update
	*/
	/*class Flexible: public SingleChildContainer {
		DEFINE_CLASS_META(Flexible, SingleChildContainer)
	public:

		Flexible(Widget* inParent, float inFlexFactor, const std::string& inID = {});

		void  DebugSerialize(Debug::PropertyArchive& inArchive) override {
			Super::DebugSerialize(inArchive);
			inArchive.PushProperty("FlexFactor", m_FlexFactor);
		}

		float GetFlexFactor() const { return m_FlexFactor; }

	private:
		float m_FlexFactor;
	};*/



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
		Clip,		// Just clip without scrolling
		Wrap,		// Wrap children on another line
		ShrinkWrap, // Do not decrease containter size below content size
					// Could be used with Scrolled widget to scroll content on overflow
	};

	struct FlexboxBuilder: public WidgetBuilder<FlexboxBuilder> {
		// Direction of the main axis		
		FlexboxBuilder& Direction(ContentDirection inDirection) { m_Direction = inDirection; return *this; }
		FlexboxBuilder& DirectionRow() { m_Direction = ContentDirection::Row; return *this; }
		FlexboxBuilder& DirectionColumn() { m_Direction = ContentDirection::Column; return *this; }
		// Distribution of children on the main axis		
		FlexboxBuilder& JustifyContent(UI::JustifyContent inJustify) { m_JustifyContent = inJustify; return *this; }
		// Alignment on the cross axis		
		FlexboxBuilder& Alignment(AlignContent inAlignment) { m_Alignment = inAlignment; return *this; }
		// What to do when children don't fit into container		
		FlexboxBuilder& OverflowPolicy(OverflowPolicy inPolicy) { m_OverflowPolicy = inPolicy; return *this; }		
		FlexboxBuilder& ExpandMainAxis(bool bExpand) { m_ExpandMainAxis = bExpand; return *this; }
		FlexboxBuilder& ExpandCrossAxis(bool bExpand) { m_ExpandCrossAxis = bExpand; return *this; }
		FlexboxBuilder& Expand() { m_ExpandCrossAxis = true; m_ExpandMainAxis = true; return *this; }

		// Finalizes creation
		Flexbox*		Create(Widget* inParent, WidgetSlot inSlot = DefaultWidgetSlot);

	public:
		
		FlexboxBuilder()
			: m_Direction(ContentDirection::Row)
			, m_JustifyContent(JustifyContent::Start)
			, m_Alignment(AlignContent::Start)
			, m_OverflowPolicy(OverflowPolicy::Clip)
			, m_ExpandMainAxis(true)
			, m_ExpandCrossAxis(false)
		{
			StyleClass("Container");
		}

		friend class Flexbox;

	public:
		UI::ContentDirection	m_Direction;
		UI::JustifyContent		m_JustifyContent;
		UI::AlignContent		m_Alignment;
		UI::OverflowPolicy		m_OverflowPolicy;
		bool					m_ExpandMainAxis;
		bool					m_ExpandCrossAxis;
	};

	/*
	* Vertical or horizontal flex container
	* Similar to Flutter and CSS
	* Default size behavior is shrink to fit children but can be set 
	*/
	class Flexbox: public MultiChildContainer {
		WIDGET_CLASS(Flexbox, MultiChildContainer)
	public:

		static FlexboxBuilder Column() { FlexboxBuilder out; out.Direction(ContentDirection::Column); return out; }
		static FlexboxBuilder Row() { FlexboxBuilder out; out.Direction(ContentDirection::Row); return out; }
		static FlexboxBuilder Build() { return {}; }

		Flexbox(const FlexboxBuilder& inDesc);

		bool OnEvent(IEvent* inEvent) override;
		void DebugSerialize(Debug::PropertyArchive& inArchive) override;

		ContentDirection GetDirection() const { return m_Direction; }

	private:

		void UpdateLayout();

	private:
		ContentDirection	m_Direction;
		JustifyContent		m_JustifyContent;
		AlignContent		m_Alignment;
		OverflowPolicy		m_OverflowPolicy;
	};

	DEFINE_ENUM_TOSTRING_2(ContentDirection, Column, Row)
	DEFINE_ENUM_TOSTRING_5(JustifyContent, Start, End, Center, SpaceBetween, SpaceAround)
	DEFINE_ENUM_TOSTRING_3(AlignContent, Start, End, Center)
	DEFINE_ENUM_TOSTRING_2(OverflowPolicy, Clip, Wrap)


/*-------------------------------------------------------------------------------------------------*/
//										TEXT
/*-------------------------------------------------------------------------------------------------*/
	
	/*
	* Displays not editable text
	*/
	class Text: public LayoutWidget {
		WIDGET_CLASS(Text, LayoutWidget)
	public:

		Text(const std::string& inText, Widget* inParent, WidgetSlot inSlot = DefaultWidgetSlot);

		void SetText(const std::string& inText);
		bool OnEvent(IEvent* inEvent) override;

	private:
		const StyleClass*	m_Style;
		std::string			m_Text;
	};


/*-------------------------------------------------------------------------------------------------*/
//										BUTTON
/*-------------------------------------------------------------------------------------------------*/
	class Button;

	using OnPressedFunc = VoidFunction<bool>;

	struct ButtonBuilder: public WidgetBuilder<ButtonBuilder> {

		ButtonBuilder&	Callback(const OnPressedFunc& inCallback) { m_Callback = inCallback; return *this; }
		ButtonBuilder&	Child(Widget* inChild) { Assertm(!m_Child, "Child already set"); m_Child.reset(inChild); return *this; }
		ButtonBuilder&	Text(const std::string& inText) { Assertm(!m_Child, "Child already set"); m_Child.reset(new UI::Text(inText, nullptr)); return *this; }

		// Finalizes creation
		Button*			Create(Widget* inParent, WidgetSlot inSlot = DefaultWidgetSlot);

	public:
		ButtonBuilder() { StyleClass("Button"); }
		friend class Button;

	public:
		OnPressedFunc					m_Callback;
		// Mutable for ownership transfer while this is const
		mutable std::unique_ptr<Widget>	m_Child;
	};

	class Button: public SingleChildContainer {
		WIDGET_CLASS(Button, SingleChildContainer)
	public:

		Button(const ButtonBuilder& inBuilder);
		bool OnEvent(IEvent* inEvent) override;
		void DebugSerialize(Debug::PropertyArchive& inArchive) override {
			Super::DebugSerialize(inArchive);
			inArchive.PushProperty("ButtonState", *m_State);
		}

		void SetCallback(OnPressedFunc inCallback) { m_Callback = inCallback; }

	public:

		LayoutInfo GetLayoutInfo() const override;

	private:
		const StyleClass*	m_Style;
		StringID			m_State;
		OnPressedFunc		m_Callback;
	};



/*-------------------------------------------------------------------------------------------------*/
//										GUIDELINE
/*-------------------------------------------------------------------------------------------------*/

	/*
	* A line that can be moved by the user
	* Calls a callback when dragged
	* Moving should be handled externally
	*/
	class Guideline: public LayoutWidget {
		WIDGET_CLASS(Guideline, LayoutWidget)
	public:
		// @param float Delta - a dragged amount in one axis
		using OnDraggedFunc = VoidFunction<MouseDragEvent*>;

		Guideline(bool bIsVertical, OnDraggedFunc inCallback, Widget* inParent, WidgetSlot inSlot = DefaultWidgetSlot);
		void SetCallback(OnDraggedFunc inCallback) { m_Callback = inCallback; }
		bool OnEvent(IEvent* inEvent) override;

	public:
		void DebugSerialize(Debug::PropertyArchive& inArchive) override;

	private:
		Axis			m_MainAxis;
		StringID		m_State;
		OnDraggedFunc	m_Callback;
	};


/*-------------------------------------------------------------------------------------------------*/
//										SPLITBOX
/*-------------------------------------------------------------------------------------------------*/

	/*
	* A container that has 3 children: First, Second and Separator
	* The user can drag the Separator to resize First and Second children
	* It's expanded on both axes
	* Children must also be expanded on both axes
	*/
	class SplitBox: public Container {
		DEFINE_CLASS_META(SplitBox, Container)
	public:

		constexpr static inline WidgetSlot FirstSlot{1};
		constexpr static inline WidgetSlot SecondSlot{2};

		SplitBox(bool bHorizontal, Widget* inParent, WidgetSlot inSlot = DefaultWidgetSlot);

		bool OnEvent(IEvent* inEvent) override;
		bool DispatchToChildren(IEvent* inEvent) override;
		void DebugSerialize(Debug::PropertyArchive& inArchive) override;
		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive) override;

		void OnSeparatorDragged(MouseDragEvent* inDragEvent);
		void SetSplitRatio(float inRatio);

	private:

		void UpdateLayout();
		
	private:
		std::unique_ptr<Guideline>		m_First;
		std::unique_ptr<Guideline>		m_Second;
		std::unique_ptr<Guideline>		m_Separator;
		Axis							m_MainAxis;
		float							m_SplitRatio;
	};




/*-------------------------------------------------------------------------------------------------*/
//										TOOLTIP
/*-------------------------------------------------------------------------------------------------*/
	/*
	* Simple container for tooltip
	* Wraps it's content
	*/
	class Tooltip: public Window {
		WIDGET_CLASS(Tooltip, Window)
	public:

		Tooltip(Point inPos);
		bool OnEvent(IEvent* inEvent) override;
	};

	// Builds simple text tooltip
	inline auto TextTooltip(const std::string& inText) {
		return [=](Point inPos) {
			auto tooltip = new Tooltip(inPos + float2(15));
			auto text = new UI::Text(inText, tooltip);
			return std::unique_ptr<Tooltip>(tooltip);
		};
	}

	
	class TooltipSpawner;

	using TooltipSpawnFunc = std::function<std::unique_ptr<Tooltip>(Point inMousePosGlobal)>;

	class SpawnTooltipIntent final: public IEvent {
		EVENT_CLASS(SpawnTooltipIntent, IEvent)
	public:
		EventCategory GetCategory() const override { return EventCategory::Intent; }

		SpawnTooltipIntent(TooltipSpawner* inSpawner)
			: Spawner(inSpawner) {}

		TooltipSpawner* Spawner = nullptr;
	};

	class CloseTooltipIntent final: public IEvent {
		EVENT_CLASS(CloseTooltipIntent, IEvent)
	public:
		EventCategory GetCategory() const override { return EventCategory::Intent; }

		CloseTooltipIntent(Tooltip* inTooltip)
			: Tooltip(inTooltip) {}

		Tooltip* Tooltip = nullptr;
	};


	/*
	* Wrapper that spawns a user defined tooltip when hovered for some time
	*/
	class TooltipSpawner: public Wrapper {
		WIDGET_CLASS(TooltipSpawner, Wrapper)
	public:

		TooltipSpawner(TooltipSpawnFunc inSpawner)
			: m_Spawner(inSpawner)
		{}

		bool OnEvent(IEvent* inEvent) override;

		std::unique_ptr<Tooltip> OnSpawn(Point inMousePosGlobal);

	private:

		void CloseTooltip();

		enum class State {
			Normal,
			Waiting,
			Active,
			Disabled,
		};

	private:
		static inline State		s_State = State::Normal;
		static inline Tooltip*	s_Tooltip = nullptr;
		TooltipSpawnFunc		m_Spawner;
	};



		
/*-------------------------------------------------------------------------------------------------*/
//										POPUP
/*-------------------------------------------------------------------------------------------------*/
	class PopupWindow;
	class PopupSpawner;
	struct PopupBuilder;

	struct PopupSpawnContext {
		Point			MousePosGlobal;
		float2			ViewportSize;
		PopupSpawner*	Spawner;
	};
		

	/*
	* A popup window created by a PopupSpawner and owned by Application but can
	* be closed by the spawner
	* Children should be wrapped in a PopupItem in order to send notifications to this window
	*/
	class PopupWindow: public Window {
		WIDGET_CLASS(PopupWindow, Window)
	public:

		PopupWindow(const PopupBuilder& inBuilder);
		~PopupWindow();
		bool OnEvent(IEvent* inEvent) override;

		// Called by a child spawner when a child widget is hovered
		void					OnItemHovered();
		void					OnItemPressed();
		// Creates a popup and parents it to itself
		WeakPtr<PopupWindow>	OpenPopup(PopupSpawner* inSpawner);

		void SetSpawner(PopupSpawner* inSpawner) { m_Spawner = inSpawner; }

	private:
		PopupSpawner*					m_Spawner;
		std::unique_ptr<PopupWindow>	m_NextPopup;
		// A widget that spawned the next popup
		// Should be our child
		PopupSpawner*					m_NextPopupSpawner;
	};

	struct PopupBuilder: public WidgetBuilder<PopupBuilder> {

		PopupBuilder& Position(Point inPos) { m_Pos = inPos; return *this; }
		PopupBuilder& Position(float inX, float inY) { m_Pos = {inX, inY}; return *this; }
		PopupBuilder& Size(float2 inSize) { m_Size = inSize; return *this; }
		PopupBuilder& Size(float inX, float inY) { m_Size = {inX, inY}; return *this; }
		std::unique_ptr<PopupWindow> Create() { return std::make_unique<UI::PopupWindow>(*this); }

	public:
		PopupBuilder() { StyleClass("PopupWindow"); }

	public:
		Point		  m_Pos;
		float2		  m_Size;
	};


	// Called by the Application to create a PopupWindow
	using PopupSpawnFunc = std::function<std::unique_ptr<PopupWindow>(PopupSpawnContext)>;

	/*
	* Propagated up the widget stack until a PopupWindow or Application is found
	*/
	class PopupEvent final: public IEvent {
		EVENT_CLASS(PopupEvent, IEvent)
	public:
		EventCategory GetCategory() const override { return EventCategory::Intent; }

		static PopupEvent OpenPopup(PopupSpawner* inSpawner) {
			PopupEvent out;
			out.Type = Type::Open;
			out.Spawner = inSpawner;
			return out;
		}

		static PopupEvent ClosePopup(PopupWindow* inPopup) {
			PopupEvent out;
			out.Type = Type::Close;
			out.Popup = inPopup;
			return out;
		}

		static PopupEvent CloseAll() {
			PopupEvent out;
			out.Type = Type::CloseAll;
			return out;
		}

		enum class Type {
			Open, 
			Close,
			CloseAll,
		};

		Type			Type = Type::Open;
		PopupSpawner*	Spawner = nullptr;
		PopupWindow*	Popup = nullptr;
	};


	/*
	* Creates a user defined popup when clicked or hovered
	*/
	class PopupSpawner: public Wrapper {
		WIDGET_CLASS(PopupSpawner, Wrapper)
	public:

		struct State {
			static inline StringID Normal{"Normal"};
			static inline StringID Opened{"Opened"};
		};

		enum class SpawnEventType {
			LeftMouseRelease,
			RightMouseRelease,
			Hover,
		};

	public:

		PopupSpawner(const PopupSpawnFunc&	inSpawner, 
						   SpawnEventType	inSpawnEvent = SpawnEventType::RightMouseRelease)
			: m_Spawner(inSpawner)
			, m_State(State::Normal)
			, m_SpawnEventType(inSpawnEvent)
		{}

		PopupSpawner(const PopupSpawnFunc&	inSpawner, 
						   Widget*			inParent, 
						   WidgetSlot		inSlot = DefaultWidgetSlot,
						   SpawnEventType	inSpawnEvent = SpawnEventType::RightMouseRelease)
			: PopupSpawner(inSpawner, inSpawnEvent) 
		{
			if(inParent) inParent->Parent(this, inSlot);
		}

		bool					OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->Cast<MouseButtonEvent>()) {

				if(m_SpawnEventType == SpawnEventType::RightMouseRelease && event->Button == MouseButton::ButtonRight) {
					Super::OnEvent(inEvent);

					if(!event->bButtonPressed && m_State == State::Normal) {
						OpenPopup();
					}
					return true;
				}

				if(m_SpawnEventType == SpawnEventType::LeftMouseRelease && event->Button == MouseButton::ButtonLeft) {
					Super::OnEvent(inEvent);

					if(!event->bButtonPressed && m_State == State::Normal) {
						OpenPopup();
					}
					return true;
				}				
			}

			if(auto* hoverEvent = inEvent->Cast<HoverEvent>(); 
				hoverEvent && m_SpawnEventType == SpawnEventType::Hover) 
			{
				const auto bHandledByChild = Super::OnEvent(inEvent);

				if(bHandledByChild && m_State == State::Normal) {
					OpenPopup();
				}
				return bHandledByChild;
			}

			return Super::OnEvent(inEvent);
		}

		// Called by Application to create a popup
		std::unique_ptr<PopupWindow> OnSpawn(Point inMousePosGlobal, float2 inWindowSize) {
			auto out = m_Spawner({inMousePosGlobal, inWindowSize, this});
			out->SetSpawner(this);
			m_Popup = out->GetWeak();
			m_State = State::Opened;
			return out;
		}

		void					OnPopupDestroyed() { m_State = State::Normal; }

		// Can be called by the user to create a popup
		void					OpenPopup() {
			if(!m_Popup) {
				auto msg = PopupEvent::OpenPopup(this);
				DispatchToParent(&msg);
			}
		}

		void					ClosePopup() {
			if(m_Popup) {
				auto msg = PopupEvent::ClosePopup(*m_Popup);
				DispatchToParent(&msg);
				m_State = State::Normal;
				Assert(!m_Popup);
				m_Popup = nullptr;
			}
		}

		bool					IsOpened() const { return m_State == State::Opened; }

		WeakPtr<PopupWindow>	GetPopupWeakPtr() { return m_Popup; }

	private:
		WeakPtr<PopupWindow>	m_Popup;
		SpawnEventType			m_SpawnEventType;
		PopupSpawnFunc			m_Spawner;
		StringID				m_State;
	};


	/*
	* When clicked closes parent popup
	* Can be used to place custom widgets into a popup window
	*/
	class PopupMenuItem: public Wrapper {
		WIDGET_CLASS(PopupMenuItem, Wrapper)
	public:

		PopupMenuItem() = default;
		bool OnEvent(IEvent* inEvent) override {

			if(auto* hoverEvent = inEvent->Cast<HoverEvent>()) {
				const auto bHandledByChild = Super::OnEvent(inEvent);

				if(bHandledByChild) {
					GetParent<PopupWindow>()->OnItemHovered();
				}
				return bHandledByChild;
			}

			if(auto* event = inEvent->Cast<MouseButtonEvent>()) {
				const auto bHandledByChild = Super::OnEvent(inEvent);

				if(bHandledByChild && !event->bButtonPressed) {
					GetParent<PopupWindow>()->OnItemPressed();
				}
				return bHandledByChild;
			}
			return Super::OnEvent(inEvent);
		}

	};

	struct ContextMenuBuilder;

	/*
	* A popup with buttons and icons
	*/
	class ContextMenu: public PopupWindow {
		WIDGET_CLASS(ContextMenu, PopupWindow)
	public:

		ContextMenu(const ContextMenuBuilder& inBuilder);
		void Parent(Widget* inWidget, WidgetSlot inSlot = DefaultWidgetSlot) override;

	private:
		Flexbox* m_Container;
	};	

	/*
	* A button that's added to the ContextMenu
	*/
	class ContextMenuItem: public PopupMenuItem {
		WIDGET_CLASS(ContextMenuItem, PopupMenuItem)
	public:

		ContextMenuItem(const std::string& inText, ContextMenu* inParent);
		bool OnEvent(IEvent* inEvent) override;
	};

	/*
	* A button that opens a submenu
	*/
	class SubMenuItem: public PopupSpawner {
		WIDGET_CLASS(SubMenuItem, PopupSpawner)
	public:

		SubMenuItem(const std::string& inText, const PopupSpawnFunc& inSpawner, ContextMenu* inParent);
		bool OnEvent(IEvent* inEvent) override;
	};


	struct ContextMenuBuilder: public WidgetBuilder<ContextMenuBuilder> {
		Point m_Pos;
		mutable	std::list<std::unique_ptr<Wrapper>> m_Children;

		ContextMenuBuilder& Position(Point inPos) { m_Pos = inPos; return *this; }
		ContextMenuBuilder& Position(float inX, float inY) { m_Pos = {inX, inY}; return *this; }
		ContextMenuBuilder& MenuItem(const std::string& inText) { m_Children.emplace_back(new ContextMenuItem(inText, nullptr)); return *this; }

		ContextMenuBuilder& SubMenu(const std::string& inText, const PopupSpawnFunc& inSpawner) { 
			m_Children.emplace_back(new SubMenuItem(inText, inSpawner, nullptr)); return *this;
		}

		std::unique_ptr<ContextMenu> Create() { return std::make_unique<ContextMenu>(*this); };

		ContextMenuBuilder() { StyleClass("ContextMenu"); }
	};



	/*
	* A drawable rectangular box 
	*/
	class Frame: public SingleChildContainer{
		WIDGET_CLASS(Frame, SingleChildContainer)
	public:

		Frame() {
			SetAxisMode(AxisModeShrink);
		}

		bool OnEvent(IEvent* inEvent) override {
			
			if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
				HandleChildEvent(event);
				NotifyParentOnSizeChanged();
				return true;
			}

			if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {

				if(m_Style) {
					drawEvent->DrawList->PushBox(GetRect(), m_Style->Find<BoxStyle>());
				}				
				DispatchDrawToChildren(drawEvent);
				return true;
			}
			return Super::OnEvent(inEvent);
		}

		void SetStyle(const std::string& inStyleName) {
			m_Style = Application::Get()->GetTheme()->Find(inStyleName);
		}

	private:
		const StyleClass* m_Style = nullptr;
	};

}