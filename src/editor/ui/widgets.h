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
class PopupWindow;
struct PopupOpenContext;

// Called by the Application to create a PopupWindow
using PopupBuilderFunc = std::function<PopupWindow*(const PopupOpenContext&)>;

// Common state enums
// Used by button like widgets
struct State {
	static State Normal;
	static State Hovered;
	static State Pressed;

	constexpr operator StringID() { return str; }

private:
	constexpr State(StringID str): str(str) {}
	StringID str;
};
inline State State::Normal{"Normal"};
inline State State::Hovered{"Hovered"};
inline State State::Pressed{"Pressed"};


/*
 * Displays not editable text
 */
class TextBox: public LayoutWidget {
	WIDGET_CLASS(TextBox, LayoutWidget)
public:
	static const inline StringID defaultStyleName{"Text"};

	TextBox(const std::string& inText, StringID inStyle = defaultStyleName);

	void SetText(const std::string& inText);
	bool OnEvent(IEvent* inEvent) override;

private:
	const StyleClass* m_Style;
	std::string       m_Text;
};



class ContainerBuilder;

/*
 * A layout widget that contains a single child
 * Draws a simple rect
 */
class Container: public SingleChildLayoutWidget {
	WIDGET_CLASS(Container, SingleChildLayoutWidget)
public:

	static ContainerBuilder Build();

	bool OnEvent(IEvent* inEvent) override {
		if(auto* layoutEvent = inEvent->Cast<ParentLayoutEvent>()) {
			if(HandleParentLayoutEvent(layoutEvent)) {
				DispatchLayoutToChildren();
			}
			return true;
		}

		if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
			if(HandleChildEvent(event)) {
				DispatchLayoutToParent();
			}
			return true;
		}

		if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
			drawEvent->canvas->DrawBox(GetRect(), m_Style->FindOrDefault<BoxStyle>(m_BoxStyleName));
			if(m_bClip) 
				drawEvent->canvas->ClipRect(GetRect());
			return true;
		}
		return Super::OnEvent(inEvent);
	}

	// Sets style selector for box style used for drawing
	void SetBoxStyleName(StringID inName) {
		m_BoxStyleName = inName;
	}

protected:
	Container(StringID inStyleName)
		: Super(inStyleName, axisModeShrink) 
	{}
	friend class ContainerBuilder;

private:
	bool              m_bClip = false;
	const StyleClass* m_Style;
	StringID          m_BoxStyleName;
};

class ContainerBuilder {
public:

	auto& ID(const std::string& inID) { id = inID; return *this; }
	auto& SizeFixed(float2 inSize) { bSizeFixed = true; axisMode = axisModeFixed; size = inSize; return *this; }	
	auto& SizeMode(AxisMode inMode) { axisMode = inMode; return *this;  }
	auto& Size(float2 inSize) { size = inSize; return *this; }
	auto& SizeExpanded() { axisMode = axisModeExpand; bSizeFixed = false; return *this; }	
	auto& PositionFloat(Point inPos) { pos = inPos; bPosFloat = true; return *this; }
	auto& BoxStyle(StringID inStyleName) { boxStyleName = inStyleName; return *this; }
	auto& StyleClass(StringID inStyleName) { styleClass = inStyleName; return *this; }
	auto& ClipContent(bool bClip) { bClipContent = bClip; return *this; }
	auto& Child(Widget* inChild) { child = inChild; return *this; }

	Container* New() {
		auto* out = new Container(styleClass);
		out->SetID(id);
		out->SetSize(size);

		if(bSizeFixed) {
			out->SetAxisMode(axisModeFixed);
		} else {
			out->SetAxisMode(axisMode);
		}
		if(bPosFloat) {
			out->SetFloatLayout(true);
			out->SetOrigin(pos);
		} 
		out->m_Style = Application::Get()->GetTheme()->Find(styleClass);
		if(child)
			out->SetChild(child);
		return out;
	}

private:
	u8       bSizeFixed : 1   = false;
	u8       bPosFloat : 1    = false;
	u8       bClipContent : 1 = false;
	float2   size;
	Point    pos;
	AxisMode axisMode = axisModeShrink;
	StringID styleClass;
	StringID boxStyleName;
	StringID id;
	Widget*  child;
};

inline ContainerBuilder Container::Build() { return {}; }









class Button;
class ButtonBuilder;

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
class Button: public Controller {
	WIDGET_CLASS(Button, Controller)
public:

	static ButtonBuilder Build();

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
			if(event->button == MouseButton::ButtonLeft) {
				
				if(event->bPressed) {
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
					ButtonEvent btnEvent(this, event->button, event->bPressed);
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
		inArchive.PushProperty("ButtonState", *(StringID)m_State);
	}

protected:
	Button(const ButtonEventFunc& inEventCallback)
		: m_State(State::Normal)
		, m_EventCallback(inEventCallback) 
	{}
	friend class ButtonBuilder;

private:
	State           m_State;
	ButtonEventFunc m_EventCallback;
};

class ButtonBuilder {
public:

	auto& SizeFixed(float2 inSize) { container.SizeFixed(inSize); return *this; }	
	auto& SizeExpanded(bool bHorizontal = true, bool bVertical = true) { container.SizeExpanded(); return *this; }	
	auto& SizeMode(AxisMode inMode) { container.SizeMode(inMode); return *this;  }
	auto& StyleClass(StringID inStyleName) { styleClass = inStyleName; return *this; }
	auto& ClipContent(bool bClip) { container.ClipContent(bClip); return *this; }
	auto& Text(const std::string& inText) { text = inText; return *this; }
	auto& Tooltip(const std::string& inText) { tooltipText = inText; return *this; }
	auto& Child(Widget* inChild) { child = inChild; return *this; }

	Button* New(); 

private:
	ButtonEventFunc  callback;
	ContainerBuilder container;
	std::string      text;
	std::string 	 tooltipText;
	StringID         styleClass = "Button";
	Widget* 		 child = nullptr;
};

inline ButtonBuilder Button::Build() { return {}; }









class WindowBuilder;
/*
 * A container that could be used as a root widget
 * E.g. a background root widget resized to the underlying os window
 * Can be customized to add resizing borders, title bar, menu bar (header), status bar (footer), close button
 */
class Window: public Controller {
	WIDGET_CLASS(Window, Controller)
public:
	static WindowBuilder Build();

	void Translate(float2 inOffset) {
		FindChildOfClass<LayoutWidget>()->Translate(inOffset);
	}

	void Focus() {
		Application::Get()->BringToFront(this);
	}
};

/*
 * Helper to configure a window procedurally
 */
class WindowBuilder {
public:

	auto& DefaultFloat() { return *this; }
	auto& ID(const std::string& inID) { id = inID; return *this; }
	auto& Style(const std::string& inStyle) { style = inStyle; return *this; }
	auto& Position(Point inPos) { pos = inPos; return *this; }
	auto& Position(float inX, float inY) { pos = {inX, inY}; return *this; }
	auto& Size(float2 inSize) { size = inSize; return *this; }
	auto& Size(float inX, float inY) { size = {inX, inY}; return *this; }
	auto& Title(const std::string& inText) { titleText = inText; return *this; }
	auto& Popup(const PopupBuilderFunc& inBuilder) { popupBuilder = inBuilder; return *this; }

	auto& Children(std::vector<Widget*>&& inChildren) { 
		children.insert(children.end(), inChildren.begin(), inChildren.end()); 
		return *this; 
	}

	Window* New();

private:
	std::string                  id;
	StringID                     style = "FloatWindow";
	Point                        pos;
	float2                       size = {100, 100};
	std::string					 titleText = "Window";
	PopupBuilderFunc			 popupBuilder;
	mutable std::vector<Widget*> children;
};

inline WindowBuilder Window::Build() { return {}; }


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


/*-------------------------------------------------------------------------------------------------*/
//										FLEXIBLE
/*-------------------------------------------------------------------------------------------------*/
/*
* Simple wrapper that provides flexFactor for parent Flexbox
* Ignored by other widgets
*/
class Flexible: public SingleChildWidget {
	DEFINE_CLASS_META(Flexible, SingleChildWidget)
public:
	Flexible(float inFlexFactor, Widget* inWidget) 
		: Super("")
		, m_FlexFactor(inFlexFactor) {
		SetChild(inWidget);
	}

	void DebugSerialize(PropertyArchive& inArchive) override {
		Super::DebugSerialize(inArchive);
		inArchive.PushProperty("FlexFactor", m_FlexFactor);
	}

	float GetFlexFactor() const { return m_FlexFactor; }

private:
	float m_FlexFactor;
};



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


enum class OverflowPolicy {
	Clip,        // Just clip without scrolling
	Wrap,        // Wrap children on another line
	ShrinkWrap,  // Do not decrease containter size below content size
				 // Could be used with Scrolled widget to scroll content on overflow
};

struct FlexboxBuilder;

struct FlexboxConfig {
	static const inline StringID defaultStyleName = "Flexbox";

	std::string          id;
	StringID             style           = defaultStyleName;
	UI::ContentDirection direction       = ContentDirection::Row;
	UI::JustifyContent   justifyContent  = JustifyContent::Start;
	UI::Alignment        alignment       = Alignment::Start;
	UI::OverflowPolicy   overflowPolicy  = OverflowPolicy::Clip;
	bool                 expandMainAxis  = true;
	bool                 expandCrossAxis = false;
};

/*
 * Vertical or horizontal flex container
 * Similar to Flutter and CSS
 * Default size behavior is shrink to fit children but can be set
 */
class Flexbox: public MultiChildLayoutWidget {
	WIDGET_CLASS(Flexbox, MultiChildLayoutWidget)
public:
	static FlexboxBuilder Build();

	static Flexbox* New(const FlexboxConfig& inConfig, std::vector<Widget*>&& inChildren) { 
		auto* out = new Flexbox(inConfig); 
		for(auto* child: inChildren) { out->AddChild(child); }
		return out;
	}	

	bool OnEvent(IEvent* inEvent) override;
	void DebugSerialize(PropertyArchive& inArchive) override;

	ContentDirection GetDirection() const { return m_Direction; }

private:
	void UpdateLayout();

protected:
	Flexbox(const FlexboxConfig& inConfig);

private:
	ContentDirection m_Direction;
	JustifyContent   m_JustifyContent;
	Alignment        m_Alignment;
	OverflowPolicy   m_OverflowPolicy;
};

DEFINE_ENUM_TOSTRING_2(ContentDirection, Column, Row)
DEFINE_ENUM_TOSTRING_5(JustifyContent, Start, End, Center, SpaceBetween, SpaceAround)
DEFINE_ENUM_TOSTRING_2(OverflowPolicy, Clip, Wrap)


struct FlexboxBuilder {
	FlexboxBuilder& ID(const std::string& inID) { config.id = inID; return *this; }
	FlexboxBuilder& Style(const std::string& inStyle) { config.style = inStyle; return *this; }
	// Direction of the main axis
	FlexboxBuilder& Direction(ContentDirection inDirection) { config.direction = inDirection; return *this; }
	FlexboxBuilder& DirectionRow() { config.direction = ContentDirection::Row; return *this; }
	FlexboxBuilder& DirectionColumn() { config.direction = ContentDirection::Column; return *this; }
	// Distribution of children on the main axis
	FlexboxBuilder& JustifyContent(UI::JustifyContent inJustify) { config.justifyContent = inJustify; return *this; }
	// Alignment on the cross axis
	FlexboxBuilder& Alignment(UI::Alignment inAlignment) { config.alignment = inAlignment; return *this; }
	FlexboxBuilder& AlignCenter() { config.alignment = Alignment::Center; return *this; }
	FlexboxBuilder& AlignStart() { config.alignment = Alignment::Start; return *this; }
	FlexboxBuilder& AlignEnd() { config.alignment = Alignment::End; return *this; }
	// What to do when children don't fit into container
	FlexboxBuilder& OverflowPolicy(OverflowPolicy inPolicy) { config.overflowPolicy = inPolicy; return *this; }
	FlexboxBuilder& ExpandMainAxis(bool bExpand = true) { config.expandMainAxis = bExpand; return *this; }
	FlexboxBuilder& ExpandCrossAxis(bool bExpand = true) { config.expandCrossAxis = bExpand; return *this; }
	FlexboxBuilder& Expand() { config.expandCrossAxis = true; config.expandMainAxis  = true; return *this; }

	FlexboxBuilder& Children(std::vector<Widget*>&& inChildren) { children.insert(children.end(), inChildren.begin(), inChildren.end()); return *this; }

	// Finalizes creation
	Flexbox* 		New() { return Flexbox::New(config, std::move(children)); }

private:
	FlexboxConfig config;
	mutable std::vector<Widget*> children;
};
inline FlexboxBuilder Flexbox::Build() { return {}; }






/*
* Aligns a child inside the parent if parent is bigger that child
*/
class Aligned: public SingleChildLayoutWidget {
	WIDGET_CLASS(Aligned, SingleChildLayoutWidget)
public:

	Aligned(Alignment inHorizontal, Alignment inVertical, Widget* inWidget) 
		: Super("", axisModeExpand) 
		, m_Horizontal(inHorizontal)
		, m_Vertical(inVertical) {
		SetChild(inWidget);
		SetAxisMode(axisModeFixed);
	}

	bool OnEvent(IEvent* inEvent) override {
		
		if(auto* layoutEvent = inEvent->Cast<ParentLayoutEvent>()) {
			auto parentAxisMode = layoutEvent->parent->GetAxisMode();

			for(auto axis: Axes2D) {
				if(parentAxisMode[axis] == AxisMode::Expand) {
					SetSize(axis, layoutEvent->constraints.Size()[axis]);
				}
			}			
			SetOrigin(layoutEvent->constraints.TL());

			if(auto* child = Super::FindChildOfClass<LayoutWidget>()) {
				DispatchLayoutToChildren();
				Align(child);
			}
			return true;
		}

		if(auto* childEvent = inEvent->Cast<ChildLayoutEvent>()) {
			auto* parent = FindParentOfClass<LayoutWidget>();
			auto childOuterSize = childEvent->child->GetOuterSize();
			
			if(!parent) {
				SetSize(childOuterSize);
				return true;
			}
			auto parentAxisMode = parent->GetAxisMode();
			auto childAxisMode = childEvent->child->GetAxisMode();

			for(auto axis: Axes2D) {
				if(parentAxisMode[axis] == AxisMode::Shrink) {
					SetSize(axis, childOuterSize[axis]);
				}
			}
			Align(childEvent->child);
			DispatchLayoutToParent();
			return true;
		}
		return Super::OnEvent(inEvent);
	}

private:

	void Align(LayoutWidget* inChild) {
		const auto innerSize      = GetInnerSize();
		const auto childMargins   = inChild->GetLayoutStyle()->margins;
		const auto childOuterSize = inChild->GetSize() + childMargins.Size();
		Point      childPos;

		for(auto axis: Axes2D) {
			auto alignment = axis == Axis::Y ? m_Vertical : m_Horizontal;

			if(alignment == Alignment::Center) {
				childPos[axis] = (innerSize[axis] - childOuterSize[axis]) * 0.5f;
			} else if(alignment == Alignment::End) {
				childPos[axis] = innerSize[axis] - childOuterSize[axis];
			}
		}
		inChild->SetOrigin(childPos + childMargins.TL());
	}

private:
	Alignment m_Horizontal;
	Alignment m_Vertical;
};








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
class TooltipPortal;

struct TooltipBuildContext {
	Point 			mousePosGlobal;
	TooltipPortal* 	sourceWidget{};
};

using TooltipBuilderFunc = std::function<Widget*(const TooltipBuildContext&)>;

/*
 * Builds a simple text tooltip
 */
struct TextTooltipBuilder {
	TextTooltipBuilder& Text(const std::string& inText) { text = inText; return *this; }
	TextTooltipBuilder& ClipText(bool bClip) { bClipText = bClip; return *this; }
	TextTooltipBuilder& StyleClass(StringID inStyleName) { styleClass = inStyleName; return *this; }

	Widget* New() {
		return ContainerBuilder()
			.StyleClass(styleClass)
			.ClipContent(bClipText)
			.Child(new UI::TextBox(text))
			.New();
	}

private:
	StringID    styleClass = "Tooltip";
	std::string text;
	bool        bClipText = false;
};

/*
 * Opens a tooltip when hovered
 */
class TooltipPortal: public MouseRegion {
	WIDGET_CLASS(TooltipPortal, MouseRegion)
public:

	TooltipPortal(const TooltipBuilderFunc& inBuilder, Widget* inChild)
		: MouseRegion(MouseRegionConfig{
			.onMouseLeave = [this]() { OnMouseLeave(); },
			.onMouseHover = [this](const auto& e) { OnMouseHover(e); }, 
			.onMouseButton = [this](const auto& e) { OnMouseButton(e); },
			.bHandleHoverAlways = true,
			.bHandleButtonAlways = true}
		)
		, m_Builder(inBuilder) {
		SetChild(inChild);
	}

	void DebugSerialize(PropertyArchive& ar) override {
		Super::DebugSerialize(ar);
		ar.PushProperty("Timer", (uintptr_t)sharedState.timerHandle);
		ar.PushProperty("Tooltip", sharedState.widget ? sharedState.widget->GetDebugID() : "nullptr");
		ar.PushProperty("bDisabled", sharedState.bDisabled);
	}

private:
	void OnMouseButton(const MouseButtonEvent& inEvent) {
		CloseTooltip();
		if(sharedState.timerHandle) {
			Application::Get()->RemoveTimer(sharedState.timerHandle);
			sharedState.timerHandle = {};
		}
		sharedState.bDisabled = true;
	}

	void OnMouseHover(const HoverEvent& inEvent) {
		constexpr unsigned delayMs = 500;

		const bool bOverlapping = sharedState.portal && sharedState.portal != this;
		// If this tooltip area covers another tooltip area
		// For example a tab and a tab close button with different tooltips
		if(bOverlapping && inEvent.bHoverEnter) {
			// Reset state and close tooltip
			if(sharedState.timerHandle) {
				Application::Get()->RemoveTimer(sharedState.timerHandle);
				sharedState.timerHandle = {};
			} else if(sharedState.widget) {
				CloseTooltip();
			}
			sharedState.bDisabled = false;
		}
		const bool bStateClean = !sharedState.bDisabled && !sharedState.widget && !sharedState.timerHandle;

		if(bStateClean) {
			const auto timerCallback = [this]() -> bool {
				OpenTooltip();
				return false;
			};
			sharedState.timerHandle = Application::Get()->AddTimer(this, timerCallback, delayMs);
			sharedState.portal = this;
			return;
		}
	}

	void OnMouseLeave() {
		if(sharedState.timerHandle) {
			Application::Get()->RemoveTimer(sharedState.timerHandle);
			sharedState.timerHandle = {};
		} else if(sharedState.widget) {
			CloseTooltip();
		}
		sharedState.bDisabled = false;
		sharedState.portal = nullptr;
	}

	void OpenTooltip() {
		auto ctx           = Application::Get()->GetState();
		sharedState.widget = m_Builder(TooltipBuildContext{ctx.mousePosGlobal, this});
		auto* layout       = LayoutWidget::FindNearest(sharedState.widget);
		Assertf(layout, "Tooltip widget {} has no LayoutWidget child, so it won't be visible.", sharedState.widget->GetDebugID());

		layout->SetOrigin(ctx.mousePosGlobal + float2(15));
		layout->SetFloatLayout(true);
		Application::Get()->Parent(sharedState.widget, Layer::Overlay);
		sharedState.timerHandle = {};
	}

	void CloseTooltip() {
		if(sharedState.widget) {
			sharedState.widget->Destroy();
			sharedState.widget = nullptr;
		}
	}

private:
	// We use static shared state because there's no point in 
	//     having multipler tooltips visible at once
	struct State {
		TooltipPortal*  portal{};
		TimerHandle 	timerHandle{};
		Widget* 		widget = nullptr;
		bool			bDisabled = false;
	};
	static inline State sharedState{};

	TooltipBuilderFunc m_Builder;
};




/*-------------------------------------------------------------------------------------------------*/
//										POPUP
/*-------------------------------------------------------------------------------------------------*/
class PopupWindow;
class PopupPortal;
class PopupBuilder;

struct PopupOpenContext {
	Point mousePosGlobal;
};


/*
* A popup window created by a PopupSpawner and owned by Application but can
* be closed by the spawner
* Children should be wrapped in a PopupItem in order to send notifications to this window
*/
class PopupWindow: public Controller {
	WIDGET_CLASS(PopupWindow, Controller)
public:

	bool OnEvent(IEvent* inEvent) {

		if(auto* mouseButtonEvent = inEvent->Cast<MouseButtonEvent>()) {
			// Close if clicked outside the window
			if(!FindChildOfClass<LayoutWidget>()->GetRect().Contains(mouseButtonEvent->mousePosGlobal)) {
				if(m_NextPopup) {
					m_NextPopup->Destroy();
				}
				Destroy();
				return true;
			}
			return false;
		}

		// if(auto* hoverEvent = inEvent->Cast<HoverEvent>()) {

		// 	if(m_NextPopup && m_NextPopup->OnEvent(inEvent)) {
		// 		return false;
		// 	}
		// 	auto bHandledByChildren = DispatchToChildren(inEvent);

		// 	if(!bHandledByChildren && !GetParent<PopupWindow>()) {
		// 		return true;
		// 	}
		// 	return bHandledByChildren;
		// }

		return Super::OnEvent(inEvent);
	}

	PopupWindow(PopupWindow* inPrevPopup, Point inPos, float2 inSize, std::vector<Widget*>&& inChildren);

	template<typename ...T> 
	PopupWindow(PopupWindow* inPrevPopup, Point inPos, float2 inSize, T*... inChildren) 
		: PopupWindow(inPrevPopup, inPos, inSize, std::vector<Widget*>({inChildren...})) 
	{}

private:
	PopupWindow* m_PrevPopup;
	WeakPtr<PopupWindow> m_NextPopup;
};

/*
 * Creates a user defined popup when clicked or hovered
 */
class PopupPortal: public MouseRegion {
	WIDGET_CLASS(PopupPortal, MouseRegion)
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

	PopupPortal(const PopupBuilderFunc&	inBuilder, Widget* inChild)
		: MouseRegion(MouseRegionConfig{
			.onMouseButton = [this](const auto& e) { OnMouseButton(e); },
			.bHandleHoverAlways = true,
		}) 
		, m_Builder(inBuilder)
	{
		SetChild(inChild);
	}
	
	void OnMouseButton(const MouseButtonEvent& inEvent) {
		if(inEvent.button == MouseButton::ButtonRight && !inEvent.bPressed) {
			OpenPopup();
		}
	}
	
	// Can be called by the user to create a popup
	void OpenPopup() {
		if(!m_Popup) {
			const auto mousePos = Application::Get()->GetState().mousePosGlobal;
			auto* out = m_Builder(PopupOpenContext{.mousePosGlobal = mousePos});
			m_Popup = out->GetWeak();
			Application::Get()->Parent(out);
		}
	}

	void ClosePopup() {
		if(m_Popup) {
			m_Popup->Destroy();
			m_Popup = nullptr;
		}
	}

	bool                 IsOpened() const { return !m_Popup; }
	WeakPtr<PopupWindow> GetPopupWeakPtr() { return m_Popup; }

private:
	WeakPtr<PopupWindow> m_Popup;
	PopupBuilderFunc     m_Builder;
};


class PopupMenuItemBuilder {
public:
	
	auto& Text(const std::string& inText) { text = inText; return *this; }
	auto& Shortcut(const std::string& inText) { shortcut = inText; return *this; }
	auto& OnPressed(const ButtonEventFunc& inCallback) { onPressed = inCallback; return *this; }

	auto* New() {
		return MouseRegion::Build()
			.OnMouseEnter([]() {
				// FindChildOfClass<Container>()->SetBoxStyleName(State::Hovered);
			})
			.OnMouseLeave([]() {
				//FindChildOfClass<Container>()->SetBoxStyleName(State::Normal);
			})
			.OnMouseButton([](const MouseButtonEvent& e) {

			})
			.Child(Container::Build()
				.StyleClass("Button") 
				.BoxStyle("Normal")
				.SizeMode({AxisMode::Expand, AxisMode::Shrink})
				.Child(Flexbox::Build()
					.DirectionRow()
					.Children({
						new Flexible(7, 
							new Aligned(Alignment::Start, Alignment::Start, 
								new TextBox(text)
							)
						),
						new Flexible(3, 
							new Aligned(Alignment::End, Alignment::Start, 
								new TextBox(shortcut)
							)
						)
					})
					.New()
				)
				.New()
			)
			.New();


		// return Button::Build()
		// 	.SizeMode({AxisMode::Expand, AxisMode::Shrink}) 
		// 	.StyleClass("Button")
		// 	.Child(
		// 		Flexbox::Build()
		// 			.DirectionRow()
		// 			.Children({
		// 				new Flexible(7, 
		// 					new Aligned(Alignment::Start, Alignment::Start, 
		// 						new TextBox(text)
		// 					)
		// 				),
		// 				new Flexible(3, 
		// 					new Aligned(Alignment::End, Alignment::Start, 
		// 						new TextBox(shortcut)
		// 					)
		// 				)
		// 			})
		// 			.New()
		// 	)
		// 	.New();
	}

private:
	ButtonEventFunc onPressed;
	std::string     text;
	std::string     shortcut;
};

}  // namespace UI