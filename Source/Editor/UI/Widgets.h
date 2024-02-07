#pragma once
#include "Widget.h"

namespace UI {

	class Style;
	class TextStyle;
	class BoxStyle;
	class LayoutStyle;

	class Flexbox;	

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
		Centered(Widget* inParent, const std::string& inID = {});
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

		// Finalizes creation
		Flexbox*		Parent(Widget* inParent);

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
		void Parent(Widget* inChild) override;

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

	class Text: public LayoutWidget {
		DEFINE_CLASS_META(Text, LayoutWidget)
	public:

		Text(Widget* inParent, const std::string& inText, const std::string& inID = {});

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
		ButtonBuilder&	Child(Widget* inChild) { m_Child = inChild; return *this; }
		// Finalizes creation
		Button*			Parent(Widget* inParent);

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
		static Button* TextButton(Widget* inParent, const std::string& inText, OnPressedFunc inCallback = {}) {
			auto* btn = new Button(inParent, inCallback, inText);
			auto* txt = new Text(btn, inText);
			return btn;
		}

		Button(Widget* inParent, OnPressedFunc inCallback = {}, const std::string& inID = {});
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

		Guideline(Widget* inParent, bool bIsVertical = true, OnDraggedFunc inCallback = {}, const std::string& inID = {});
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




	/*
	* A container that has 3 children: First, Second and Separator
	* The user can drag the Separator to resize First and Second children
	* It's expanded on both axes
	* Children must also be expanded on both axes
	*/
	class SplitBox: public Container {
		DEFINE_CLASS_META(SplitBox, Container)
	public:

		SplitBox(Widget* inParent, bool bHorizontal = true, const std::string& inID = {});

		void Parent(Widget* inChild) override;
		void Unparent(Widget* inChild) override;
		bool OnEvent(IEvent* inEvent) override;
		bool DispatchToChildren(IEvent* inEvent) override;
		void DebugSerialize(Debug::PropertyArchive& inArchive) override;
		VisitResult VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive) override;

		void OnSeparatorDragged(MouseDragEvent* inDragEvent);
		void SetSplitRatio(float inRatio);

	private:

		void UpdateLayout();
		
	private:
		AxisIndex					m_MainAxis;
		float						m_SplitRatio;
		std::unique_ptr<Widget>		m_First;
		std::unique_ptr<Widget>		m_Second;
		std::unique_ptr<Guideline>	m_Separator;
	};






	class Tooltip: public SingleChildContainer {
		DEFINE_CLASS_META(Tooltip, SingleChildContainer)
	public:

		static Tooltip* Text(const std::string& inText) {
			auto tooltip = new Tooltip();
			auto text = new UI::Text(tooltip, inText);
			return tooltip;
		}

		Tooltip(float2 inSize = {});
		bool OnEvent(IEvent* inEvent) override;
	};


	/*
	* Wrapper that spawns a user defined tooltip when hovered for some time
	*/
	class TooltipSpawner: public Controller {
		DEFINE_CLASS_META(TooltipSpawner, Controller)
	public:

		TooltipSpawner(Widget* inParent, const SpawnerFunction& inSpawner);
		LayoutWidget* Spawn();

	private:
		SpawnerFunction m_Spawner;
	};












	///*
	//* Maps button events into controlled widget visibility change
	//* Each button aka tab controls one widget
	//* 
	//*/
	//class TabController: public Controller {
	//	DEFINE_CLASS_META(TabController, Controller)
	//public:

	//	TabController(Widget* inParent, u32 inDefaultTabIndex = 0, const std::string& inID = {});
	//	bool OnEvent(IEvent* inEvent) override;

	//	void BindButton(Button* inWidget, u32 inControlIndex);
	//	void BindWindow(Widget* inControlled, u32 inControlIndex);

	//private:

	//	struct Binding {
	//		Button* Button = nullptr;
	//		Widget* Controlled = nullptr;
	//	};

	//	std::vector<Binding> m_Controls;
	//};


















	/*struct ListDesc {
		ContentDirection	Direction;
		std::list<Widget*>	Children;
	};*/

	/*
	* Vertical or horizontal scrollable container
	* Children should have fixed size on the main axis
	*/
	/*class List: public Container {
	public:

		List(const ListDesc& inDesc);
	};*/





	/*
	* In order to create a tooltip for some widget
	* just wrap that widget in this class
	*/
	//class Tooltip: public SingleChildContainer {
	//	DEFINE_CLASS_META(Tooltip, SingleChildContainer)
	//public:

	//	Tooltip(SpawnerFunction inTooltipSpawnerFunction);
	//	// Called by framework after the child has
	//	//	been hovered for some time
	//	// Returns a tooltip widget with the actual data. 
	//	//	e.g. text inside a box
	//	// Lifetime of that object is managed by framework
	//	Widget* Spawn() const;
	//};






	//class ContextMenu: public MultiChildContainer {
	//	DEFINE_CLASS_META(ContextMenu, MultiChildContainer)
	//public:

	//	ContextMenu();

	//	//void OnOpen(const PopupContext& inContext);
	//};


	/*
	* In order to create a popup for some widget
	* just wrap that widget in this class
	*/
	/*class PopupSpawner: public SingleChildContainer {
		DEFINE_CLASS_META(Popup, SingleChildContainer)
	public:

		PopupSpawner(SpawnerFunction inPopupSpawnerFunction);

		Widget* Spawn() const;
	};*/





	/*
	* The user is supposed to override this class
	* In order to create a drag & drop for some widget
	* just wrap that widget in this class
	*/
	//class DragDropSource: public SingleChildContainer {
	//	DEFINE_CLASS_META(DragDrop, SingleChildContainer)
	//public:

	//	using SpawnerFunction = std::function<Widget* ()>;

	//	DragDropSource(const std::string& inType, const std::string& inPayload, SpawnerFunction inPreviewSpawner);

	//	bool OnEvent(IEvent* inEvent) override {

	//		if(auto* event = inEvent->CastTo<MouseMoveEvent>()) {
	//			// App.StartDragDrop(this, SpawnPreview());
	//			return true;
	//		}
	//		return Super::OnEvent(inEvent);
	//	}
	//};

	/*class DragDropTarget: public SingleChildContainer {
		DEFINE_CLASS_META(DragDrop, SingleChildContainer)
	public:

		DragDropTarget(const std::string& inType, VoidFunction<std::string> inOnAcceptFunction);

		bool OnHover(const std::string& inType);
		void OnAccept(const std::string& inPayload);
	};*/
}