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

		Window(Application* inApp, const std::string& inID, WindowFlags inFlags, const std::string& inStyleClassName = "Window");
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
		Centered(WidgetAttachSlot& inSlot, const std::string& inID = {});
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
		Flexbox*		Create(WidgetAttachSlot& inSlot);

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

		Text(WidgetAttachSlot& inSlot, const std::string& inText, const std::string& inID = {});
		Text(const std::string& inText, const std::string& inID = {});

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

	struct ButtonDesc {
		Widget* Child = nullptr;
	};

	struct ButtonBuilder: public WidgetBuilder<ButtonBuilder> {

		ButtonBuilder&	Callback(const OnPressedFunc& inCallback) { m_Callback = inCallback; return *this; }
		ButtonBuilder&	Child(Widget* inChild) { Assertm(!m_Child, "Child already set"); m_Child.reset(inChild); return *this; }
		ButtonBuilder&	Text(const std::string& inText) { Assertm(!m_Child, "Child already set"); m_Child.reset(new UI::Text(inText)); return *this; }

		// Finalizes creation
		Button*			Create(WidgetAttachSlot& inSlot);

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

		static ButtonBuilder Build() { return {}; }

		// Helper named constructor to create a simple button with the text
		static Button* TextButton(WidgetAttachSlot& inSlot, const std::string& inText, OnPressedFunc inCallback = {}) {
			auto* btn = new Button(inSlot, inCallback, inText);
			auto* txt = new Text(btn->DefaultSlot, inText);
			return btn;
		}

		Button(WidgetAttachSlot& inSlot, OnPressedFunc inCallback = {}, const std::string& inID = {});
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

		Guideline(WidgetAttachSlot& inSlot, bool bIsVertical = true, OnDraggedFunc inCallback = {}, const std::string& inID = {});
		void SetCallback(OnDraggedFunc inCallback) { m_Callback = inCallback; }
		bool OnEvent(IEvent* inEvent) override;

	public:
		void DebugSerialize(Debug::PropertyArchive& inArchive) override;

	private:
		AxisIndex		m_MainAxis;
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

		SplitBox(WidgetAttachSlot& inSlot, bool bHorizontal = true, const std::string& inID = {});

		bool OnEvent(IEvent* inEvent) override;
		bool DispatchToChildren(IEvent* inEvent) override;
		void DebugSerialize(Debug::PropertyArchive& inArchive) override;
		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive) override;

		void OnSeparatorDragged(MouseDragEvent* inDragEvent);
		void SetSplitRatio(float inRatio);

	private:

		void UpdateLayout();
		
	public:

		SingleWidgetSlot<LayoutWidget>	FirstSlot;
		SingleWidgetSlot<LayoutWidget>	SecondSlot;

	private:
		SingleWidgetSlot<Guideline>		m_Separator;
		AxisIndex						m_MainAxis;
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
	auto TextTooltip(const std::string& inText) {
		return [=](Point inPos) {
			auto tooltip = new Tooltip(inPos + float2(15));
			auto text = new UI::Text(tooltip->DefaultSlot, inText);
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

	class CloseTooltipIntent final: public IEvent{
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
	class PopupItem;
	class PopupWindow;
	class PopupSpawner;

	struct PopupBuilder: public WidgetBuilder<PopupBuilder> {

		PopupBuilder& Position(Point inPos) { m_Pos = inPos; return *this; }
		PopupBuilder& Position(float inX, float inY) { m_Pos = {inX, inY}; return *this; }
		PopupBuilder& Size(float2 inSize) { m_Size = inSize; return *this; }
		PopupBuilder& Size(float inX, float inY) { m_Size = {inX, inY}; return *this; }
		std::unique_ptr<PopupWindow> Create();

	public:
		PopupBuilder() { StyleClass("PopupWindow"); }
		friend class Window;

	public:
		Point		  m_Pos;
		float2		  m_Size;
	};

	/*
	* A popup window created by a PopupSpawner and owned by Application but can
	* be closed by the spawner
	* Children should be wrapped in a PopupItem in order to send notifications to this window
	*/
	class PopupWindow: public Window {
		WIDGET_CLASS(PopupWindow, Window)
	public:

		PopupWindow(const std::string& inID);
		PopupWindow(const PopupBuilder& inBuilder);
		~PopupWindow();
		bool OnEvent(IEvent* inEvent) override;

		// Called by a child spawner when a child widget is hovered
		void OnPopupItemHovered(PopupItem* inPopupItem);
		void OnPopupItemPressed(PopupItem* inPopupItem);

		void SetSpawner(PopupSpawner* inSpawner) { m_Spawner = inSpawner; }

	private:
		PopupSpawner*					m_Spawner;
		SingleWidgetSlot<PopupWindow>	m_NextPopup;
		// A widget that spawned the next popup
		// Should be our child
		PopupSpawner*					m_NextPopupSpawner;
	};


	// Called by the Application to create a PopupWindow
	using PopupSpawnFunc = 
		std::function<std::unique_ptr<PopupWindow>(Point inMousePosGlobal, float2 inViewportSize, PopupSpawner* inSpawner)>;

	/*
	* An event propagated up
	* Handled by the Application or PopupWindow
	*/
	class SpawnPopupIntent final: public IEvent {
		EVENT_CLASS(SpawnPopupIntent, IEvent)
	public:
		EventCategory GetCategory() const override { return EventCategory::Intent; }

		SpawnPopupIntent(PopupSpawner* inSpawner)
			: Spawner(inSpawner)
		{}

		PopupSpawner* Spawner = nullptr;
	};

	class ClosePopupIntent final: public IEvent {
		EVENT_CLASS(ClosePopupIntent, IEvent)
	public:
		EventCategory GetCategory() const override { return EventCategory::Intent; }

		ClosePopupIntent(PopupWindow* inPopup)
			: Popup(inPopup) {}

		PopupWindow* Popup = nullptr;
	};


	/*
	* Creates a user defined popup
	*/
	class PopupSpawner: public Wrapper {
		WIDGET_CLASS(PopupSpawner, Wrapper)
	public:

		struct State {
			static inline StringID Normal{"Normal"};
			static inline StringID Opened{"Opened"};
		};

		enum class SpawnEvent {
			LeftMouseRelease,
			RightMouseRelease,
			Hover,
		};

	public:

		PopupSpawner(const PopupSpawnFunc& inSpawner, SpawnEvent inSpawnEvent = SpawnEvent::RightMouseRelease)
			: m_Spawner(inSpawner)
			, m_State(State::Normal)
			, m_SpawnEvent(inSpawnEvent)
		{}

		PopupSpawner(WidgetAttachSlot& inSlot, const PopupSpawnFunc& inSpawner, SpawnEvent inSpawnEvent = SpawnEvent::RightMouseRelease)
			: PopupSpawner(inSpawner, inSpawnEvent)
		{ 
			inSlot.Parent(this);
		}

		bool			OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->Cast<MouseButtonEvent>()) {

				if(m_SpawnEvent == SpawnEvent::RightMouseRelease && event->Button == MouseButton::ButtonRight) {
					Super::OnEvent(inEvent);

					if(!event->bButtonPressed && m_State == State::Normal) {
						SpawnPopupIntent event{this};
						DispatchToParent(&event);
					}
					return true;
				}

				if(m_SpawnEvent == SpawnEvent::LeftMouseRelease && event->Button == MouseButton::ButtonLeft) {
					Super::OnEvent(inEvent);

					if(!event->bButtonPressed && m_State == State::Normal) {
						SpawnPopupIntent event{this};
						DispatchToParent(&event);
					}
					return true;
				}				
			}

			if(auto* hoverEvent = inEvent->Cast<HoverEvent>(); 
				hoverEvent && m_SpawnEvent == SpawnEvent::Hover) 
			{
				const auto bHandledByChild = Super::OnEvent(inEvent);

				if(bHandledByChild && m_State == State::Normal) {
					SpawnPopupIntent event{this};
					DispatchToParent(&event);
				}
				return bHandledByChild;
			}

			return Super::OnEvent(inEvent);
		}

		// Called by Application to create a popup
		std::unique_ptr<PopupWindow> OnSpawn(Point inMousePosGlobal, float2 inWindowSize) {
			auto out = m_Spawner(inMousePosGlobal, inWindowSize, this);
			out->SetSpawner(this);
			m_State = State::Opened;
			return out;
		}

		void			OnPopupDestroyed() { m_State = State::Normal; }

		// Can be called by the user to create a popup
		void			OpenPopup() {
			SpawnPopupIntent event{this};
			Super::DispatchToParent(&event);
		}

		bool			IsOpened() const { return m_State == State::Opened; }

	private:
		SpawnEvent		m_SpawnEvent;
		PopupSpawnFunc	m_Spawner;
		StringID		m_State;
	};



	/*
	* When clicked closes parent popup
	*/
	class PopupMenuItem: public Wrapper {
		WIDGET_CLASS(PopupMenuItem, Wrapper)
	public:

		PopupMenuItem() {}

		bool OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->Cast<MouseButtonEvent>()) {
				const auto bHandledByChild = Super::OnEvent(inEvent);

				if(bHandledByChild && !event->bButtonPressed) {
					ClosePopupIntent closePopup(Super::GetParent<PopupWindow>());
					Super::DispatchToParent(&closePopup);
				}
				return bHandledByChild;
			}
			return Super::OnEvent(inEvent);
		}

	};

}