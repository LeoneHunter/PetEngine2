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
class PopupState;
struct PopupOpenContext;

// Called by the Application to create a PopupWindow
using PopupBuilderFunc = std::function<std::vector<std::unique_ptr<WidgetState>>(const PopupOpenContext&)>;

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

	static std::unique_ptr<TextBox> New(const std::string& inText, StringID inStyle = defaultStyleName) {
		return std::make_unique<TextBox>(inText, inStyle);
	}
	TextBox(const std::string& inText, StringID inStyle = defaultStyleName);

	// TODO: remove because should be changed on rebuild
	void SetText(const std::string& inText);
	bool OnEvent(IEvent* inEvent) override;
	float2 OnLayout(const ParentLayoutEvent& inEvent) override;

private:
	const StyleClass* m_Style;
	std::string       m_Text;
};



class ContainerBuilder;

/*
 * A layout widget that contains a single child
 * Draws a simple rect
 * TODO: rework drawing and style changes
 */
class Container: public SingleChildLayoutWidget {
	WIDGET_CLASS(Container, SingleChildLayoutWidget)
public:

	static ContainerBuilder Build();

	bool OnEvent(IEvent* inEvent) override {
		if(auto* layoutEvent = inEvent->As<ParentLayoutEvent>()) {
			if(HandleParentLayoutEvent(layoutEvent)) {
				DispatchLayoutToChildren();
			}
			return true;
		}

		if(auto* event = inEvent->As<ChildLayoutEvent>()) {
			if(HandleChildEvent(event)) {
				DispatchLayoutToParent();
			}
			return true;
		}

		if(auto* drawEvent = inEvent->As<DrawEvent>()) {
			drawEvent->canvas->DrawBox(GetRect(), m_Style->FindOrDefault<BoxStyle>(m_BoxStyleName));
			if(m_bClip) 
				drawEvent->canvas->ClipRect(GetRect());
			return true;
		}
		return Super::OnEvent(inEvent);
	}

	// TODO: maybe change on rebuild
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
	auto& ClipContent(bool bClip = true) { bClipContent = bClip; return *this; }
	auto& Child(std::unique_ptr<Widget>&& inChild) { child = std::move(inChild); return *this; }

	std::unique_ptr<Container> New() {
		std::unique_ptr<Container> out(new Container(styleClass));
		out->SetID(id);
		out->SetSize(size);
		out->m_bClip = bClipContent;

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
		if(child) {
			out->Parent(std::move(child));
		}
		return out;
	}

private:
	u8                      bSizeFixed : 1   = false;
	u8                      bPosFloat : 1    = false;
	u8                      bClipContent : 1 = false;
	float2                  size;
	Point                   pos;
	AxisMode                axisMode = axisModeShrink;
	StringID                styleClass;
	StringID                boxStyleName;
	StringID                id;
	std::unique_ptr<Widget> child;
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

using ButtonEventFunc = std::function<void(const ButtonEvent&)>;

/*
 * Simple clickable button with flat style
 * TODO: maybe use composited widgets from MouseRegion and Container instead
 * and use some bindings or notifications
 */
class Button: public Controller {
	WIDGET_CLASS(Button, Controller)
public:

	static ButtonBuilder Build();

	bool OnEvent(IEvent* inEvent) override {
		if(auto* event = inEvent->As<HoverEvent>()) {
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
		if(auto* event = inEvent->As<MouseButtonEvent>()) {
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
					m_EventCallback(btnEvent);
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
	auto& AlignContent(Alignment x, Alignment y) { alignX = x; alignY = y; return *this; }
	auto& Text(const std::string& inText) { text = inText; return *this; }
	auto& Tooltip(const std::string& inText) { tooltipText = inText; return *this; }
	auto& OnPress(const ButtonEventFunc& inCallback) { callback = inCallback; return *this; }
	auto& Child(std::unique_ptr<Widget>&& inChild) { child = std::move(inChild); return *this; }

	std::unique_ptr<Button> New(); 

private:
	Alignment				alignX = Alignment::Start;
	Alignment				alignY = Alignment::Start;
	ButtonEventFunc         callback;
	ContainerBuilder        container;
	std::string             text;
	std::string             tooltipText;
	StringID                styleClass = "Button";
	std::unique_ptr<Widget> child;
};

inline ButtonBuilder Button::Build() { return {}; }









// class WindowBuilder;
// /*
//  * A container that could be used as a root widget
//  * E.g. a background root widget resized to the underlying os window
//  * Can be customized to add resizing borders, title bar, menu bar (header), status bar (footer), close button
//  */
// class Window: public Controller {
// 	WIDGET_CLASS(Window, Controller)
// public:
// 	static WindowBuilder Build();

// 	void Translate(float2 inOffset) {
// 		FindChildOfClass<LayoutWidget>()->Translate(inOffset);
// 	}

// 	void Focus() {
// 		Application::Get()->BringToFront(this);
// 	}
// };

// /*
//  * Helper to configure a window procedurally
//  */
// class WindowBuilder {
// public:

// 	auto& DefaultFloat() { return *this; }
// 	auto& ID(const std::string& inID) { id = inID; return *this; }
// 	auto& Style(const std::string& inStyle) { style = inStyle; return *this; }
// 	auto& Position(Point inPos) { pos = inPos; return *this; }
// 	auto& Position(float inX, float inY) { pos = {inX, inY}; return *this; }
// 	auto& Size(float2 inSize) { size = inSize; return *this; }
// 	auto& Size(float inX, float inY) { size = {inX, inY}; return *this; }
// 	auto& Title(const std::string& inText) { titleText = inText; return *this; }
// 	auto& Popup(const PopupBuilderFunc& inBuilder) { popupBuilder = inBuilder; return *this; }

// 	auto& Children(std::vector<Widget*>&& inChildren) { 
// 		children.insert(children.end(), inChildren.begin(), inChildren.end()); 
// 		return *this; 
// 	}

// 	Window* New();

// private:
// 	std::string                  id;
// 	StringID                     style = "FloatWindow";
// 	Point                        pos;
// 	float2                       size = {100, 100};
// 	std::string					 titleText = "Window";
// 	PopupBuilderFunc			 popupBuilder;
// 	mutable std::vector<Widget*> children;
// };

// inline WindowBuilder Window::Build() { return {}; }


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

	static auto New(float inFlexFactor, std::unique_ptr<Widget>&& inChild) {
		return std::make_unique<Flexible>(inFlexFactor, std::move(inChild));
	}

	Flexible(float inFlexFactor, std::unique_ptr<Widget>&& inWidget) 
		: Super("")
		, m_FlexFactor(inFlexFactor) {
		Parent(std::move(inWidget));
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
class FlexboxBuilder;

/*
 * Vertical or horizontal flex container
 * Similar to Flutter and CSS
 * Default size behavior is shrink to fit children but can be set
 */
class Flexbox: public MultiChildLayoutWidget {
	WIDGET_CLASS(Flexbox, MultiChildLayoutWidget)
public:
	friend class FlexboxBuilder;

	static FlexboxBuilder Build();

	float2 OnLayout(const ParentLayoutEvent& inEvent) override;
	bool OnEvent(IEvent* inEvent) override;
	void DebugSerialize(PropertyArchive& inArchive) override;

	ContentDirection GetDirection() const { return m_Direction; }

private:
	void LayOutChildren(const ParentLayoutEvent& inEvent);

private:
	ContentDirection m_Direction;
	JustifyContent   m_JustifyContent;
	Alignment        m_Alignment;
	OverflowPolicy   m_OverflowPolicy;
};

DEFINE_ENUM_TOSTRING_2(ContentDirection, Column, Row)
DEFINE_ENUM_TOSTRING_5(JustifyContent, Start, End, Center, SpaceBetween, SpaceAround)
DEFINE_ENUM_TOSTRING_2(OverflowPolicy, Clip, Wrap)


class FlexboxBuilder {
public:
	FlexboxBuilder& ID(const std::string& inID) { id = inID; return *this; }
	FlexboxBuilder& Style(const std::string& inStyle) { style = inStyle; return *this; }
	// Direction of the main axis
	FlexboxBuilder& Direction(ContentDirection inDirection) { direction = inDirection; return *this; }
	FlexboxBuilder& DirectionRow() { direction = ContentDirection::Row; return *this; }
	FlexboxBuilder& DirectionColumn() { direction = ContentDirection::Column; return *this; }
	// Distribution of children on the main axis
	FlexboxBuilder& JustifyContent(UI::JustifyContent inJustify) { justifyContent = inJustify; return *this; }
	// Alignment on the cross axis
	FlexboxBuilder& Alignment(UI::Alignment inAlignment) { alignment = inAlignment; return *this; }
	FlexboxBuilder& AlignCenter() { alignment = Alignment::Center; return *this; }
	FlexboxBuilder& AlignStart() { alignment = Alignment::Start; return *this; }
	FlexboxBuilder& AlignEnd() { alignment = Alignment::End; return *this; }
	// What to do when children don't fit into container
	FlexboxBuilder& OverflowPolicy(OverflowPolicy inPolicy) { overflowPolicy = inPolicy; return *this; }
	FlexboxBuilder& ExpandMainAxis(bool bExpand = true) { expandMainAxis = bExpand; return *this; }
	FlexboxBuilder& ExpandCrossAxis(bool bExpand = true) { expandCrossAxis = bExpand; return *this; }
	FlexboxBuilder& Expand() { expandCrossAxis = true; expandMainAxis  = true; return *this; }

	FlexboxBuilder& Children(std::vector<std::unique_ptr<Widget>>&& inChildren) { 
		for(auto& child: inChildren) {
			children.push_back(std::move(child));
		}
		return *this; 
	}
	
	template<typename ...T> 
		requires (std::derived_from<T, Widget> && ...)
	FlexboxBuilder& Children(std::unique_ptr<T>&& ... inChild) { 
		(children.push_back(std::move(inChild)), ...);
		return *this; 
	}

	// Finalizes creation
	std::unique_ptr<Flexbox> New();

private:
	static const inline StringID defaultStyleName = "Flexbox";

	std::string          id;
	StringID             style           = defaultStyleName;
	UI::ContentDirection direction       = ContentDirection::Row;
	UI::JustifyContent   justifyContent  = JustifyContent::Start;
	UI::Alignment        alignment       = Alignment::Start;
	UI::OverflowPolicy   overflowPolicy  = OverflowPolicy::Clip;
	bool                 expandMainAxis  = true;
	bool                 expandCrossAxis = false;

	std::vector<std::unique_ptr<Widget>> children;
};
inline FlexboxBuilder Flexbox::Build() { return {}; }






/*
* Aligns a child inside the parent if parent is bigger that child
*/
class Aligned: public SingleChildLayoutWidget {
	WIDGET_CLASS(Aligned, SingleChildLayoutWidget)
public:

	static auto New(Alignment inHorizontal, Alignment inVertical, std::unique_ptr<Widget>&& inChild) {
		return std::make_unique<Aligned>(inHorizontal, inVertical, std::move(inChild));
	}

	Aligned(Alignment inHorizontal, Alignment inVertical, std::unique_ptr<Widget>&& inWidget) 
		: Super("", axisModeExpand) 
		, m_Horizontal(inHorizontal)
		, m_Vertical(inVertical) {
		Parent(std::move(inWidget));
		SetAxisMode(axisModeFixed);
	}

	float2 OnLayout(const ParentLayoutEvent& inEvent) override {
		auto* parent = FindParentOfClass<LayoutWidget>();
		auto parentAxisMode = parent ? parent->GetAxisMode() : axisModeExpand;
		SetOrigin(inEvent.constraints.TL() + GetLayoutStyle()->margins.TL());

		for(auto axis: Axes2D) {
			if(parentAxisMode[axis] == AxisMode::Expand || parentAxisMode[axis] == AxisMode::Fixed) {
				SetSize(axis, inEvent.constraints.Size()[axis]);
			}
		}
		if(auto* child = FindChildOfClass<LayoutWidget>()) {
			const auto childSize = child->OnLayout(inEvent);
			
			for(auto axis: Axes2D) {
				if(parentAxisMode[axis] == AxisMode::Shrink) {
					SetSize(axis, childSize[axis]);
				}
			}
			Align(child);
		}
		return GetOuterSize();
	}

	// TODO: deprecated
	bool OnEvent(IEvent* inEvent) override {
		
		if(auto* layoutEvent = inEvent->As<ParentLayoutEvent>()) {
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

		if(auto* childEvent = inEvent->As<ChildLayoutEvent>()) {
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
		const auto innerSize      = GetSize();
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

using TooltipBuilderFunc = std::function<std::unique_ptr<Widget>(const TooltipBuildContext&)>;

/*
 * Builds a simple text tooltip
 */
struct TextTooltipBuilder {
	TextTooltipBuilder& Text(const std::string& inText) { text = inText; return *this; }
	TextTooltipBuilder& ClipText(bool bClip) { bClipText = bClip; return *this; }
	TextTooltipBuilder& StyleClass(StringID inStyleName) { styleClass = inStyleName; return *this; }

	std::unique_ptr<Widget> New() {
		return Container::Build()
			.StyleClass(styleClass)
			.ClipContent(bClipText)
			.Child(TextBox::New(text))
			.New();
	}

private:
	StringID    styleClass = "Tooltip";
	std::string text;
	bool        bClipText = false;
};

/*
 * Opens a tooltip when hovered
 * TODO: maybe convert to state and use MouseRegion and notifications instead
 */
class TooltipPortal: public MouseRegion {
	WIDGET_CLASS(TooltipPortal, MouseRegion)
public:

	static auto New(const TooltipBuilderFunc& inBuilder, std::unique_ptr<Widget>&& inChild) {
		return std::make_unique<TooltipPortal>(inBuilder, std::move(inChild));
	}

	TooltipPortal(const TooltipBuilderFunc& inBuilder, std::unique_ptr<Widget>&& inChild)
		: MouseRegion(MouseRegionConfig{
			.onMouseLeave = [this]() { OnMouseLeave(); },
			.onMouseHover = [this](const auto& e) { OnMouseHover(e); }, 
			.onMouseButton = [this](const auto& e) { OnMouseButton(e); },
			.bHandleHoverAlways = true,
			.bHandleButtonAlways = true}
		)
		, m_Builder(inBuilder) {
		Parent(std::move(inChild));
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

		// FIXME: doesn't work properly on rebuild
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
		auto widget 	   = m_Builder(TooltipBuildContext{ctx.mousePosGlobal, this});
		sharedState.widget = widget.get();
		auto* layout       = LayoutWidget::FindNearest(sharedState.widget);
		Assertf(layout, "Tooltip widget {} has no LayoutWidget child, so it won't be visible.", sharedState.widget->GetDebugID());

		layout->SetOrigin(ctx.mousePosGlobal + float2(15));
		layout->SetFloatLayout(true);
		Application::Get()->Parent(std::move(widget), Layer::Overlay);
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
class PopupPortal;
class PopupBuilder;
class PopupState;

// TODO: maybe remove
struct PopupOpenContext {
	Point       mousePosGlobal;
	// Current state that builds the popup
	PopupState* current = nullptr;
};

using PopupMenuItemOnPressFunc = std::function<void()>;

// Created by the user inside the builder callback
// Owned by PopupState
class PopupMenuItem: public WidgetState {
	STATE_CLASS(PopupMenuItem, WidgetState)
public:
	PopupMenuItem(const std::string&              inText,
				  const std::string&              inShortcut,
				  const PopupMenuItemOnPressFunc& inOnPress)
		: m_Text(inText)
		, m_Shortcut(inShortcut)
		, m_OnPress(inOnPress)
		, m_Parent(nullptr)
		, m_Index(0)
	{}

	void Bind(PopupState* inParent, u32 inIndex) { m_Parent = inParent; m_Index = inIndex; }

	std::unique_ptr<Widget> Build() override;

private:
	std::string              m_Text;
	std::string              m_Shortcut;
	PopupMenuItemOnPressFunc m_OnPress;
	PopupState*              m_Parent;
	u32 					 m_Index;
};

// Opens a submenu
class PopupSubmenuItem: public WidgetState {
	STATE_CLASS(PopupSubmenuItem, WidgetState)
public:
	PopupSubmenuItem(const std::string&      inText,
				     const PopupBuilderFunc& inBuilder)
		: m_Text(inText)
		, m_Builder(inBuilder)
		, m_Parent(nullptr)
		, m_Index(0)
	{}

	void Bind(PopupState* inParent, u32 inIndex) { m_Parent = inParent; m_Index = inIndex; }
	std::unique_ptr<Widget> Build() override;
	const PopupBuilderFunc& GetBuilder() const { return m_Builder; }

private:
	std::string      m_Text;
	PopupState*      m_Parent;
	u32              m_Index;
	PopupBuilderFunc m_Builder;
};

/*
* TODO: Add collisions and positioning. Mayve use LayouNotifications and ScreenCollider that uses callbacks
* Contains and builds a list of user provided menu items in the inBuilder function
* Owner is a PopupPortal or another PopupState of the previous popup
*/
class PopupState: public WidgetState {
	STATE_CLASS(PopupState, WidgetState)
public:

	PopupState(const PopupBuilderFunc& inBuilder, PopupPortal* inOwner, Point inPos)
		: m_Portal(inOwner) 
		, m_Pos(inPos) {

		m_Items = inBuilder(PopupOpenContext{});
		u32 index = 0;

		for(auto& item: m_Items) {
			if(auto* i = item->As<PopupMenuItem>()) {
				i->Bind(this, index);
			} else if(auto* i = item->As<PopupSubmenuItem>()) {
				i->Bind(this, index);
			} else {
				Assertm(false, "Popup menu content items should be of class PopupMenuItem or PopupSubmenuItem");
			}
			++index;
		}
	}

	PopupState(const PopupBuilderFunc& inBuilder, PopupState* inOwner, Point inPos)
	 	: PopupState(inBuilder, (PopupPortal*)nullptr, inPos) {
		m_PreviousPopup = inOwner;
	}

	std::unique_ptr<Widget> Build() override {
		std::vector<std::unique_ptr<Widget>> out;
		u32 index = 0;

		for(auto& item: m_Items) {
			out.push_back(StatefulWidget::New(item.get()));
		}
		auto body = Container::Build()
			.PositionFloat(m_Pos)
			.SizeMode({AxisMode::Fixed, AxisMode::Shrink})
			.Size(float2{300, 0})
			.ID("PopupBody")
			.StyleClass("Popup")
			.Child(Flexbox::Build()
				.DirectionColumn()
				.ExpandCrossAxis(true)
				.ExpandMainAxis(false)
				.Children(std::move(out))
				.New())
			.New();
		// Container with the size of the screen to capture all mouse events
		if(m_PreviousPopup) {
			return body;
		} else {
			return MouseRegion::Build()
				.OnMouseButton([this](const auto&) { Close(); })
				.Child(Container::Build()
					.ID("PopupScreenCapture")
					.StyleClass("Transparent")
					.SizeExpanded()
					.Child(std::move(body))
					.New()
				)
				.New();
		}
	}

	void OnItemHovered(u32 inItemIndex, bool bEnter) {
		// TODO: Open submenu here
	}

	void OnItemPressed(u32 inItemIndex) { 
		Assert(inItemIndex < m_Items.size());		
		if(auto* item = m_Items[inItemIndex]->As<PopupSubmenuItem>()) {
			OpenSubmenu(*item, inItemIndex);
		} else {
			Close(); 
		}
	}

	void Close(); 

private:

	void OpenSubmenu(PopupSubmenuItem& inItem, u32 inItemIndex) {
		if(m_NextPopup && m_NextPopupItemIndex != inItemIndex) {
			m_NextPopup.reset();
		}
		if(!m_NextPopup) {
			auto* layoutWidget = inItem.GetWidget()->FindChildOfClass<LayoutWidget>();
			auto  rectGlobal = layoutWidget->GetRectGlobal();
			m_NextPopup = std::make_unique<PopupState>(inItem.GetBuilder(), this, rectGlobal.TR());
			m_NextPopupItemIndex = inItemIndex;
			Application::Get()->Parent(StatefulWidget::New(m_NextPopup.get()));
		}
	}

	void CloseSubmenu() {
		if(m_NextPopup) {
			m_NextPopup.reset();
		}
	}

private:
	Point 					    m_Pos;
	// Either a PopupPortal or another Popup is our owner
	PopupPortal*                m_Portal = nullptr;
	PopupState*                 m_PreviousPopup = nullptr;
	std::unique_ptr<PopupState> m_NextPopup;
	u32							m_NextPopupItemIndex = 0;
	// Items provided by the user
	std::vector<std::unique_ptr<WidgetState>> m_Items;
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

	static auto New(const PopupBuilderFunc&	inBuilder, std::unique_ptr<Widget>&& inChild) {
		return std::make_unique<PopupPortal>(inBuilder, std::move(inChild));
	}

	PopupPortal(const PopupBuilderFunc&	inBuilder, std::unique_ptr<Widget>&& inChild)
		: MouseRegion(MouseRegionConfig{
			.onMouseButton = [this](const auto& e) { OnMouseButton(e); },
			.bHandleHoverAlways = true,
		})
		, m_Builder(inBuilder) {
		Parent(std::move(inChild));
	}
	
	void OnMouseButton(const MouseButtonEvent& inEvent) {
		if(inEvent.button == MouseButton::ButtonRight && !inEvent.bPressed) {
			OpenPopup();
		}
	}
	
	// Can be called by the user to create a popup
	void OpenPopup() {
		if(!m_Popup) {
			auto pos = Application::Get()->GetState().mousePosGlobal;
			m_Popup = std::make_unique<PopupState>(m_Builder, this, pos);
			Application::Get()->Parent(StatefulWidget::New(m_Popup.get()));
		}
	}

	void ClosePopup() {
		if(m_Popup) {
			m_Popup.reset();
		}
	}

	bool IsOpened() const { return !m_Popup; }

private:
	// The state is owned by us but the stateful widgets that uses this state
	// is owned by the Application
	std::unique_ptr<PopupState> m_Popup;
	PopupBuilderFunc            m_Builder;
};


}  // namespace UI


inline std::unique_ptr<UI::Widget> UI::PopupMenuItem::Build() {
	// TODO: use Container instead of Button and change the style on rebuild
	return MouseRegion::Build()
		.OnMouseEnter([this]() { m_Parent->OnItemHovered(m_Index, true); })
		.OnMouseLeave([this]() { m_Parent->OnItemHovered(m_Index, false); })
		.Child(Button::Build()
			.StyleClass("PopupMenuItem") 
			.SizeMode({AxisMode::Expand, AxisMode::Shrink})
			.OnPress([this](const ButtonEvent& e) {
				if(e.button == MouseButton::ButtonLeft && !e.bPressed) {
					this->m_OnPress();
					this->m_Parent->OnItemPressed(m_Index); 
				}
			})
			.Child(Flexbox::Build()
				.DirectionRow()
				.Children(
					Flexible::New(7, 
						Aligned::New(Alignment::Start, Alignment::Start, 
							TextBox::New(m_Text)
						)
					),
					Flexible::New(3, 
						Aligned::New(Alignment::End, Alignment::Start, 
							TextBox::New(m_Shortcut)
						)
					)
				)
				.New()
			)
			.New()
		)
		.New();
}

inline std::unique_ptr<UI::Widget> UI::PopupSubmenuItem::Build() {
	return MouseRegion::Build()
		.OnMouseEnter([this]() { m_Parent->OnItemHovered(m_Index, true); })
		.OnMouseLeave([this]() { m_Parent->OnItemHovered(m_Index, false); })
		.Child(Button::Build()
			.StyleClass("PopupMenuItem") 
			.SizeMode({AxisMode::Expand, AxisMode::Shrink})
			.OnPress([this](const ButtonEvent& e) {
				if(e.button == MouseButton::ButtonLeft && !e.bPressed) {
					this->m_Parent->OnItemPressed(m_Index); 
				}
			})
			.Child(Flexbox::Build()
				.DirectionRow()
				.Children(
					Flexible::New(7, 
						Aligned::New(Alignment::Start, Alignment::Start, 
							TextBox::New(m_Text)
						)
					),
					Flexible::New(3, 
						Aligned::New(Alignment::End, Alignment::Start, 
							TextBox::New(">")
						)
					)
				)
				.New()
			)
			.New()
		)
		.New();
}
	
inline void UI::PopupState::Close() { 
	if(m_Portal) {
		m_Portal->ClosePopup(); 
	} else if(m_PreviousPopup) {
		m_PreviousPopup->Close();
	} else {
		Assertm(false, "PopupState has no owner.");
	}
}