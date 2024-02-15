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
		Window*					Parent(Application* inApp);
		std::unique_ptr<Window> Create();

	public:

		WindowBuilder(): m_Flags(WindowFlags::None) {
			StyleClass("Window");
		}

		friend class Window;

	private:
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
		DEFINE_CLASS_META(Window, SingleChildContainer)
	public:

		Window(Application* inApp, const std::string& inID, WindowFlags inFlags);
		Window(const WindowBuilder& inBuilder);
		bool OnEvent(IEvent* inEvent) override;

		bool HasAnyWindowFlags(WindowFlags inFlags) const { return m_Flags & inFlags; }

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
		DEFINE_CLASS_META(Centered, SingleChildContainer)
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
		Flexbox*		Parent(WidgetAttachSlot& inSlot);

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

	private:
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
		DEFINE_CLASS_META(Flexbox, MultiChildContainer)
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
		DEFINE_CLASS_META(Text, LayoutWidget)
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

	struct ButtonState {
		static inline StringID Normal{"Normal"};
		static inline StringID Hovered{"Hovered"};
		static inline StringID Pressed{"Pressed"};
	};

	struct ButtonBuilder: public WidgetBuilder<ButtonBuilder> {

		ButtonBuilder&	Callback(const OnPressedFunc& inCallback) { m_Callback = inCallback; return *this; }
		ButtonBuilder&	Child(Widget* inChild) { Assertm(!m_Child, "Child already set"); m_Child = inChild; return *this; }
		ButtonBuilder&	Text(const std::string& inText) { Assertm(!m_Child, "Child already set"); m_Child = new UI::Text(inText); return *this; }

		// Finalizes creation
		Button*			Parent(WidgetAttachSlot& inSlot);

	public:

		ButtonBuilder()
			: m_Child(nullptr)
		{
			StyleClass("Button");
		}

		friend class Button;

	private:
		OnPressedFunc	m_Callback;
		Widget*			m_Child;
	};

	class Button: public SingleChildContainer {
		DEFINE_CLASS_META(Button, SingleChildContainer)
	public:

		static ButtonBuilder Build() { return {}; }

		// Helper named constructor to create a simple button with the text
		static Button* TextButton(WidgetAttachSlot& inSlot, const std::string& inText, OnPressedFunc inCallback = {}) {
			auto* btn = new Button(inSlot, inCallback, inText);
			auto* txt = new Text(btn->ChildSlot, inText);
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

		Padding GetPaddings() const override;
		Padding GetMargins() const override;

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
		DEFINE_CLASS_META(Guideline, LayoutWidget)
	public:

		// @param float Delta - a dragged amount in one axis
		using OnDraggedFunc = VoidFunction<MouseDragEvent*>;

		Guideline(WidgetAttachSlot& inSlot, bool bIsVertical = true, OnDraggedFunc inCallback = {}, const std::string& inID = {});
		void SetCallback(OnDraggedFunc inCallback) { m_Callback = inCallback; }

		bool OnEvent(IEvent* inEvent) override;

	public:

		void DebugSerialize(Debug::PropertyArchive& inArchive) override {
			Super::DebugSerialize(inArchive);
			inArchive.PushProperty("MainAxis", !m_MainAxis ? "X" : "Y");
			inArchive.PushProperty("State", m_State.String());
		}

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
	class Tooltip: public SingleChildContainer {
		DEFINE_CLASS_META(Tooltip, SingleChildContainer)
	public:

		static Tooltip* Text(const std::string& inText) {
			auto tooltip = new Tooltip();
			auto text = new UI::Text(tooltip->ChildSlot, inText);
			return tooltip;
		}

		Tooltip(float2 inSize = {});
		bool OnEvent(IEvent* inEvent) override;

		Padding GetPaddings() const override;
		Padding GetMargins() const override;

	private:
		const StyleClass* m_Style;
	};


	/*
	* Wrapper that spawns a user defined tooltip when hovered for some time
	*/
	class TooltipSpawner: public Controller {
		DEFINE_CLASS_META(TooltipSpawner, Controller)
	public:

		static TooltipSpawner* Text(const std::string& inText) {
			return new TooltipSpawner([=]() { return Tooltip::Text(inText); });
		}

		TooltipSpawner(WidgetAttachSlot& inSlot, const SpawnerFunction& inSpawner);
		TooltipSpawner(const SpawnerFunction& inSpawner);
		std::unique_ptr<LayoutWidget> Spawn();

	private:
		SpawnerFunction m_Spawner;

	};


		
/*-------------------------------------------------------------------------------------------------*/
//										POPUP
/*-------------------------------------------------------------------------------------------------*/
	using PopupSpawnFunc = std::function<std::unique_ptr<Window>(Point inMousePosGlobal)>;
	using PopupDestroyFunc = std::function<void(Window* inPopup)>;

	class SpawnPopupIntent final: public IEvent {
		DEFINE_CLASS_META(SpawnPopupIntent, Controller)
	public:
		EventCategory GetCategory() const override { return EventCategory::Intent; }

		SpawnPopupIntent(PopupSpawner* inSpawner)
			: Spawner(inSpawner)
		{}

		PopupSpawner* Spawner = nullptr;
	};	

	/*
	* Creates a user defined popup
	*/
	class PopupSpawner: public Controller {
		DEFINE_CLASS_META(PopupSpawner, Controller)
	public:

		PopupSpawner(WidgetAttachSlot& inSlot, const PopupSpawnFunc& inSpawner)
			: m_Spawner(inSpawner)
		{ 
			inSlot.Parent(this); 
		}

		bool OnEvent(IEvent* inEvent) override {

			if(auto* event = inEvent->Cast<MouseButtonEvent>()) {

				if(event->Button == MouseButton::ButtonRight) {

					if(!event->bButtonPressed) {
						SpawnPopupIntent event{this};
						Super::DispatchToParent(&event);
					}
					return true;
				}				
			}
			return Super::OnEvent(inEvent);
		}

		// Can be called by user to create a popup
		void OpenPopup() {
			SpawnPopupIntent event{this};
			Super::DispatchToParent(&event);
		}

		// Called by Application to create a popup
		std::unique_ptr<Window> OnSpawn(Point inMousePosGlobal) {
			auto out = m_Spawner(inMousePosGlobal);
			m_Popup = out.get();
			return out;
		}

		void	OnDestroy() { m_Popup = nullptr; }

		Window* GetPopup() { return m_Popup; }

	private:
		PopupSpawnFunc	m_Spawner;
		Window*			m_Popup = nullptr;
	};



	/*
	* When hovered opens a popup
	*/
	class PopupMenuItem: public SingleChildContainer {
		DEFINE_CLASS_META(PopupMenuItem, SingleChildContainer)
	public:

		PopupMenuItem(WidgetAttachSlot& inSlot, const PopupSpawnFunc& inSpawnerFunc);
		bool OnEvent(IEvent* inEvent) override;

	public:

		Padding GetPaddings() const override;
		Padding GetMargins() const override;

	private:
		const StyleClass*	m_Style;
		StringID			m_State;
	};

}