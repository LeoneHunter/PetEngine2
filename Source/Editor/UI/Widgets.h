#pragma once
#include "Widget.h"

namespace UI {

	class Style;
	class TextStyle;
	class BoxStyle;
	class LayoutStyle;

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



	class Expanded: public SingleChildContainer {
		DEFINE_CLASS_META(Expanded, SingleChildContainer)
	public:
		Expanded(Widget* inParent, const std::string& inID = {});
		void OnParented(Widget* inParent) override;
		bool OnEvent(IEvent* inEvent) override;

	private:
		AxisIndex GetMainAxis() const;
	};



	/*
	* TODO
	*	Handle size update
	*/
	class Flexible: public SingleChildContainer {
		DEFINE_CLASS_META(Flexible, SingleChildContainer)
	public:

		Flexible(float inFlexFactor): m_FlexFactor(inFlexFactor) {}
		float GetFlexFactor() const { return m_FlexFactor; }

	private:
		float m_FlexFactor;
	};



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
		Scroll,		// Enable scrolling
		Wrap,		// Wrap children on another line
		ShrinkWrap, // Do not decrease containter size below content size
	};

	struct FlexboxDesc {
		std::string			ID;
		// Direction of the main axis
		ContentDirection	Direction = ContentDirection::Row;
		// Distribution of children on the main axis
		JustifyContent		JustifyContent = JustifyContent::Start;
		// Alignment on the cross axis
		AlignContent		Alignment = AlignContent::Start;
		// What to do when children don't fit into container
		OverflowPolicy		OverflowPolicy = OverflowPolicy::Clip;
		bool				bExpandCrossAxis = false;
		bool				bExpandMainAxis = true;
	};

	/*
	* Vertical or horizontal flex container
	* Similar to Flutter and CSS
	* Default size behavior is shrink to fit children
	* 
	* TODO
	*	Batch updates when many children are added
	*	Add scrolling when overflown
	*/
	class Flexbox: public MultiChildContainer {
		DEFINE_CLASS_META(Flexbox, MultiChildContainer)
	public:

		static Flexbox* Column(Widget* inParent, const std::string& inID = {}, bool bExpandCrossAxis = false, bool bExpandMainAxis = true) {
			auto* out = new Flexbox(inParent, FlexboxDesc{.ID = inID, .Direction = ContentDirection::Column, .bExpandCrossAxis = bExpandCrossAxis, .bExpandMainAxis = bExpandMainAxis});
			return out;
		}

		static Flexbox* Row(Widget* inParent, const std::string& inID = {}, bool bExpandCrossAxis = false, bool bExpandMainAxis = true) {
			auto* out = new Flexbox(inParent, FlexboxDesc{.ID = inID, .Direction = ContentDirection::Row, .bExpandCrossAxis = bExpandCrossAxis, .bExpandMainAxis = bExpandMainAxis});
			return out;
		}

		Flexbox(Widget* inParent, const FlexboxDesc& inDesc);
		bool OnEvent(IEvent* inEvent) override;
		void Parent(Widget* inChild) override;

		void DebugSerialize(Debug::PropertyArchive& inArchive) override {
			Super::DebugSerialize(inArchive);
			inArchive.PushProperty("Direction", m_Direction);
			inArchive.PushProperty("JustifyContent", m_JustifyContent);
			inArchive.PushProperty("Alignment", m_Alignment);
			inArchive.PushProperty("OverflowPolicy", m_OverflowPolicy);
		}

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
	DEFINE_ENUM_TOSTRING_3(OverflowPolicy, Clip, Scroll, Wrap)





	class Text: public LayoutWidget {
		DEFINE_CLASS_META(Text, LayoutWidget)
	public:

		Text(Widget* inParent, const std::string& inText, const std::string& inID = {});

		void SetText(const std::string& inText);
		bool OnEvent(IEvent* inEvent) override;

	private:
		std::string		m_Text;
		TextStyle*		m_Style;
	};




	struct ButtonDesc {
		Widget* Child = nullptr;
	};

	enum class ButtonState {
		Default,
		Hovered,
		Pressed,
	};

	class Button: public SingleChildContainer {
		DEFINE_CLASS_META(Button, SingleChildContainer)
	public:

		using OnPressedFunc = VoidFunction<bool>;

		// Helper named constructor to create a simple button with the text
		static Button* TextButton(Widget* inParent, const std::string& inText, OnPressedFunc inCallback = {}) {
			auto* btn = new Button(inParent, inCallback);
			auto* txt = new Text(btn, inText);
			return btn;
		}

		Button(Widget* inParent, OnPressedFunc inCallback = {}, const std::string& inID = {});
		bool OnEvent(IEvent* inEvent) override;

		void DebugSerialize(Debug::PropertyArchive& inArchive) override {
			Super::DebugSerialize(inArchive);
			inArchive.PushProperty("ButtonState", m_State);
		}

		void SetCallback(OnPressedFunc inCallback) { m_Callback = inCallback; }

	private:
		ButtonState	m_State = ButtonState::Default;
		OnPressedFunc	m_Callback;
	};

	DEFINE_ENUM_TOSTRING_3(ButtonState, Default, Hovered, Pressed)




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

	private:
		AxisIndex		m_MainAxis;
		ButtonState		m_State;
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