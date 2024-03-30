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
struct StateEnum {
	static inline const StringID Normal{"Normal"};
	static inline const StringID Hovered{"Hovered"};
	static inline const StringID Pressed{"Pressed"};
	static inline const StringID Opened{"Opened"};
	static inline const StringID Selected{"Selected"};
};



/*
 * Displays not editable text
 */
class TextBox: public LayoutWidget {
	WIDGET_CLASS(TextBox, LayoutWidget)
public:
	static const inline StringID defaultStyleName{"Text"};

	static std::unique_ptr<TextBox> New(const std::string& text, StringID style = defaultStyleName) {
		return std::make_unique<TextBox>(text, style);
	}
	TextBox(const std::string& text, StringID style = defaultStyleName);

	bool OnEvent(IEvent* event) override;
	float2 OnLayout(const LayoutConstraints& event) override;

private:
	const StyleClass* style_;
	std::string       text_;
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

	bool OnEvent(IEvent* event) override {
		if(auto* drawEvent = event->As<DrawEvent>()) {
			drawEvent->canvas->DrawBox(GetRect(), style_->FindOrDefault<BoxStyle>(boxStyleName_));
			if(bClip_) 
				drawEvent->canvas->ClipRect(GetRect());
			return true;
		}
		return Super::OnEvent(event);
	}

	// TODO: maybe change on rebuild
	// Sets style selector for box style used for drawing
	void SetBoxStyleName(StringID name) {
		boxStyleName_ = name;
	}

protected:
	Container(StringID styleName, bool notifyOnLayout = false)
		: Super(styleName, axisModeShrink, notifyOnLayout) 
	{}
	friend class ContainerBuilder;

private:
	bool              bClip_ = false;
	const StyleClass* style_;
	StringID          boxStyleName_;
};

class ContainerBuilder {
public:
	auto& ID(const std::string& inID) { id = inID; return *this; }
	auto& SizeFixed(float2 inSize) { bSizeFixed = true; axisMode = axisModeFixed; size = inSize; return *this; }	
	auto& SizeMode(AxisMode inMode) { axisMode = inMode; return *this;  }
	auto& Size(float2 inSize) { size = inSize; return *this; }
	auto& SizeExpanded() { axisMode = axisModeExpand; bSizeFixed = false; return *this; }	
	auto& PositionFloat(Point inPos) { pos = inPos; bPosFloat = true; return *this; }
	auto& NotifyOnLayoutUpdate() { bNotifyOnLayout = true; return *this; }
	auto& BoxStyle(StringID inStyleName) { boxStyleName = inStyleName; return *this; }
	auto& StyleClass(StringID inStyleName) { styleClass = inStyleName; return *this; }
	auto& ClipContent(bool bClip = true) { bClipContent = bClip; return *this; }
	auto& Child(std::unique_ptr<Widget>&& inChild) { child = std::move(inChild); return *this; }

	std::unique_ptr<Container> New() {
		std::unique_ptr<Container> out(new Container(styleClass, bNotifyOnLayout));
		out->SetID(id);
		out->SetSize(size);
		out->bClip_ = bClipContent;

		if(bSizeFixed) {
			out->SetAxisMode(axisModeFixed);
		} else {
			out->SetAxisMode(axisMode);
		}
		if(bPosFloat) {
			out->SetFloatLayout(true);
			out->SetOrigin(pos);
		} 
		out->style_ = Application::Get()->GetTheme()->Find(styleClass);
		if(child) {
			out->Parent(std::move(child));
		}
		return out;
	}

private:
	u8                      bSizeFixed : 1   = false;
	u8                      bPosFloat : 1    = false;
	u8                      bClipContent : 1 = false;
	u8						bNotifyOnLayout:1 = false;
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
	ButtonEvent(Button* source, MouseButton btn, bool bPressed)
		: source(source), button(btn), bPressed(bPressed) {}

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
class Button: public SingleChildWidget {
	WIDGET_CLASS(Button, SingleChildWidget)
public:

	static ButtonBuilder Build();

	bool OnEvent(IEvent* event) override {
		if(auto* hoverEvent = event->As<HoverEvent>()) {
			// Change state and style
			if(hoverEvent->bHoverEnter) {
				state_ = StateEnum::Hovered;
				FindChildOfClass<Container>()->SetBoxStyleName(StateEnum::Hovered);

			} else if(hoverEvent->bHoverLeave) {
				state_ = StateEnum::Normal;
				FindChildOfClass<Container>()->SetBoxStyleName(StateEnum::Normal);
			}
			return true;
		}
		if(auto* mouseButtonEvent = event->As<MouseButtonEvent>()) {
			if(mouseButtonEvent->button == MouseButton::ButtonLeft) {
				
				if(mouseButtonEvent->bPressed) {
					state_ = StateEnum::Pressed;
					FindChildOfClass<Container>()->SetBoxStyleName(StateEnum::Pressed);

				} else {
					auto* container = FindChildOfClass<Container>(); 

					if(container->GetRect().Contains(mouseButtonEvent->mousePosLocal)) {
						state_ = StateEnum::Hovered;
						container->SetBoxStyleName(StateEnum::Hovered);
					} else {
						state_ = StateEnum::Normal;
						container->SetBoxStyleName(StateEnum::Normal);
					}
				}
				if(eventCallback_) {
					ButtonEvent btnEvent(this, mouseButtonEvent->button, mouseButtonEvent->bPressed);
					eventCallback_(btnEvent);
				}
				return true;
			}
			return false;
		}
		return Super::OnEvent(event);
	}

	void DebugSerialize(PropertyArchive& archive) override {
		Super::DebugSerialize(archive);
		archive.PushProperty("ButtonState", *(StringID)state_);
	}

protected:
	Button(const ButtonEventFunc& eventCallback)
		: state_(StateEnum::Normal)
		, eventCallback_(eventCallback) 
	{}
	friend class ButtonBuilder;

private:
	StringID        state_;
	ButtonEventFunc eventCallback_;
};

class ButtonBuilder {
public:

	auto& SizeFixed(float2 size) { container.SizeFixed(size); return *this; }	
	auto& SizeExpanded(bool bHorizontal = true, bool bVertical = true) { container.SizeExpanded(); return *this; }	
	auto& SizeMode(AxisMode mode) { container.SizeMode(mode); return *this;  }
	auto& StyleClass(StringID styleName) { styleClass = styleName; return *this; }
	auto& ClipContent(bool bClip) { container.ClipContent(bClip); return *this; }
	auto& AlignContent(Alignment x, Alignment y) { alignX = x; alignY = y; return *this; }
	auto& Text(const std::string& text) { this->text = text; return *this; }
	auto& Tooltip(const std::string& text) { tooltipText = text; return *this; }
	auto& OnPress(const ButtonEventFunc& callback) { this->callback = callback; return *this; }
	auto& Child(std::unique_ptr<Widget>&& child) { this->child = std::move(child); return *this; }

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

	static auto New(float flexFactor, std::unique_ptr<Widget>&& child) {
		return std::make_unique<Flexible>(flexFactor, std::move(child));
	}

	Flexible(float flexFactor, std::unique_ptr<Widget>&& widget) 
		: Super("")
		, flexFactor_(flexFactor) {
		Parent(std::move(widget));
	}

	void DebugSerialize(PropertyArchive& archive) override {
		Super::DebugSerialize(archive);
		archive.PushProperty("FlexFactor", flexFactor_);
	}

	float GetFlexFactor() const { return flexFactor_; }

private:
	float flexFactor_;
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

	float2 OnLayout(const LayoutConstraints& event) override;
	void DebugSerialize(PropertyArchive& archive) override;

	ContentDirection GetDirection() const { return direction_; }

private:
	void LayOutChildren(const LayoutConstraints& event);

private:
	ContentDirection direction_;
	JustifyContent   justifyContent_;
	Alignment        alignment_;
	OverflowPolicy   overflowPolicy_;
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
	FlexboxBuilder& Children(std::unique_ptr<T>&& ... child) { 
		(children.push_back(std::move(child)), ...);
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

	static auto New(Alignment horizontal, Alignment vertical, std::unique_ptr<Widget>&& child) {
		return std::make_unique<Aligned>(horizontal, vertical, std::move(child));
	}

	Aligned(Alignment horizontal, Alignment vertical, std::unique_ptr<Widget>&& widget) 
		: Super("", axisModeExpand) 
		, horizontal_(horizontal)
		, vertical_(vertical) {
		Parent(std::move(widget));
		SetAxisMode(axisModeFixed);
	}

	float2 OnLayout(const LayoutConstraints& event) override {
		auto* parent = FindParentOfClass<LayoutWidget>();
		auto parentAxisMode = parent ? parent->GetAxisMode() : axisModeExpand;
		SetOrigin(event.rect.TL() + GetLayoutStyle()->margins.TL());

		for(auto axis: Axes2D) {
			if(parentAxisMode[axis] == AxisMode::Expand || parentAxisMode[axis] == AxisMode::Fixed) {
				SetSize(axis, event.rect.Size()[axis]);
			}
		}
		if(auto* child = FindChildOfClass<LayoutWidget>()) {
			const auto childSize = child->OnLayout(event);
			
			for(auto axis: Axes2D) {
				if(parentAxisMode[axis] == AxisMode::Shrink) {
					SetSize(axis, childSize[axis]);
				}
			}
			Align(child);
			child->OnPostLayout();
		}
		return GetOuterSize();
	}

private:

	void Align(LayoutWidget* child) {
		const auto innerSize      = GetSize();
		const auto childMargins   = child->GetLayoutStyle()->margins;
		const auto childOuterSize = child->GetSize() + childMargins.Size();
		Point      childPos;

		for(auto axis: Axes2D) {
			auto alignment = axis == Axis::Y ? vertical_ : horizontal_;

			if(alignment == Alignment::Center) {
				childPos[axis] = (innerSize[axis] - childOuterSize[axis]) * 0.5f;
			} else if(alignment == Alignment::End) {
				childPos[axis] = innerSize[axis] - childOuterSize[axis];
			}
		}
		child->SetOrigin(childPos + childMargins.TL());
	}

private:
	Alignment horizontal_;
	Alignment vertical_;
};



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
	TextTooltipBuilder& Text(const std::string& text) { this->text = text; return *this; }
	TextTooltipBuilder& ClipText(bool bClip) { bClipText = bClip; return *this; }
	TextTooltipBuilder& StyleClass(StringID styleName) { styleClass = styleName; return *this; }

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

	static auto New(const TooltipBuilderFunc& builder, std::unique_ptr<Widget>&& child) {
		return std::make_unique<TooltipPortal>(builder, std::move(child));
	}

	TooltipPortal(const TooltipBuilderFunc& builder, std::unique_ptr<Widget>&& child)
		: MouseRegion(MouseRegionConfig{
			.onMouseLeave = [this]() { OnMouseLeave(); },
			.onMouseHover = [this](const auto& e) { OnMouseHover(e); }, 
			.onMouseButton = [this](const auto& e) { OnMouseButton(e); },
			.bHandleHoverAlways = true,
			.bHandleButtonAlways = true}
		)
		, builder_(builder) {
		Parent(std::move(child));
	}

	void DebugSerialize(PropertyArchive& ar) override {
		Super::DebugSerialize(ar);
		ar.PushProperty("Timer", (uintptr_t)sharedState.timerHandle);
		ar.PushProperty("Tooltip", sharedState.widget ? sharedState.widget->GetDebugID() : "nullptr");
		ar.PushProperty("bDisabled", sharedState.bDisabled);
	}

private:
	void OnMouseButton(const MouseButtonEvent& event) {
		CloseTooltip();
		if(sharedState.timerHandle) {
			Application::Get()->RemoveTimer(sharedState.timerHandle);
			sharedState.timerHandle = {};
		}
		sharedState.bDisabled = true;
	}

	void OnMouseHover(const HoverEvent& event) {
		constexpr unsigned delayMs = 500;

		// FIXME: doesn't work properly on rebuild
		const bool bOverlapping = sharedState.portal && sharedState.portal != this;
		// If this tooltip area covers another tooltip area
		// For example a tab and a tab close button with different tooltips
		if(bOverlapping && event.bHoverEnter) {
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
			const auto timerCallback = [this]()->bool {
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
		auto widget 	   = builder_(TooltipBuildContext{ctx.mousePosGlobal, this});
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
	struct StateEnum {
		TooltipPortal*  portal{};
		TimerHandle 	timerHandle{};
		Widget* 		widget = nullptr;
		bool			bDisabled = false;
	};
	static inline StateEnum sharedState{};

	TooltipBuilderFunc builder_;
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
	WIDGET_CLASS(PopupMenuItem, WidgetState)
public:
	PopupMenuItem(const std::string&              text,
				  const std::string&              shortcut,
				  const PopupMenuItemOnPressFunc& onPress)
		: text_(text)
		, shortcut_(shortcut)
		, onPress_(onPress)
		, parent_(nullptr)
		, index_(0)
	{}

	void Bind(PopupState* parent, u32 index) { parent_ = parent; index_ = index; }
	std::unique_ptr<Widget> Build() override;

private:
	std::string              text_;
	std::string              shortcut_;
	PopupMenuItemOnPressFunc onPress_;
	PopupState*              parent_;
	u32 					 index_;
};

// Opens a submenu
class PopupSubmenuItem: public WidgetState {
	WIDGET_CLASS(PopupSubmenuItem, WidgetState)
public:
	PopupSubmenuItem(const std::string&      text,
				     const PopupBuilderFunc& builder)
		: text_(text)
		, builder_(builder)
		, parent_(nullptr)
		, index_(0)
		, state_(StateEnum::Normal)
	{}

	void Bind(PopupState* parent, u32 index) { parent_ = parent; index_ = index; }
	std::unique_ptr<Widget> Build() override;
	const PopupBuilderFunc& GetBuilder() const { return builder_; }
	void SetOpen(bool isOpen) { isOpen_ = isOpen; }

private:
	StringID		 state_;
	bool			 isOpen_;
	std::string      text_;
	PopupState*      parent_;
	u32              index_;
	PopupBuilderFunc builder_;
};

/*
* TODO: Add collisions and positioning. Mayve use LayouNotifications and ScreenCollider that uses callbacks
* Contains and builds a list of user provided menu items in the builder function
* Owner is a PopupPortal or another PopupState of the previous popup
*/
class PopupState: public WidgetState {
	WIDGET_CLASS(PopupState, WidgetState)
public:

	PopupState(const PopupBuilderFunc& builder, PopupPortal* owner, Point pos)
		: portal_(owner) 
		, pos_(pos) {

		items_ = builder(PopupOpenContext{});
		u32 index = 0;

		for(auto& item: items_) {
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

	PopupState(const PopupBuilderFunc& builder, PopupState* owner, Point pos)
	 	: PopupState(builder, (PopupPortal*)nullptr, pos) {
		previousPopup_ = owner;
	}

	~PopupState() {
		if(timerHandle_) {
			Application::Get()->RemoveTimer(timerHandle_);
		}
	}

	std::unique_ptr<Widget> Build() override {
		std::vector<std::unique_ptr<Widget>> out;
		u32 index = 0;

		for(auto& item: items_) {
			out.push_back(StatefulWidget::New(item.get()));
		}
		auto body = EventListener::New(
			[this](IEvent* event) {
				auto* e = event->As<LayoutNotification>();
				if(e && e->depth == 0) {
					this->PositionPopup(*e);
					return true;
				}
				return false;	
			}, 
			Container::Build()
				.PositionFloat(pos_)
				.NotifyOnLayoutUpdate()
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
				.New()
		);
		// Container with the size of the screen to capture all mouse events
		if(previousPopup_) {
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

	void OnItemHovered(u32 itemIndex, bool bEnter) {
		constexpr auto timerDelayMs = 500;
		if(bEnter) {
			auto timerCallback = [this, index = itemIndex]()->bool {
				Assert(index < items_.size());		
				if(auto* item = items_[index]->As<PopupSubmenuItem>()) {
					OpenSubmenu(*item, index);
				} else {
					CloseSubmenu();
				}
				return false;
			};
			timerHandle_ = Application::Get()->AddTimer(this, timerCallback, timerDelayMs);
		} else {
			Application::Get()->RemoveTimer(timerHandle_);
		}
	}

	void OnItemPressed(u32 itemIndex) { 
		Assert(itemIndex < items_.size());		
		if(auto* item = items_[itemIndex]->As<PopupSubmenuItem>()) {
			OpenSubmenu(*item, itemIndex);
		} else {
			Close(); 
		}
	}

	void Close(); 

private:

	void OpenSubmenu(PopupSubmenuItem& item, u32 itemIndex) {
		if(nextPopup_ && nextPopupItemIndex_ != itemIndex) {
			nextPopup_.reset();
		}
		if(!nextPopup_) {
			auto* layoutWidget = item.GetWidget()->FindChildOfClass<LayoutWidget>();
			auto  rectGlobal = layoutWidget->GetRectGlobal();
			nextPopup_ = std::make_unique<PopupState>(item.GetBuilder(), this, rectGlobal.TR());
			item.SetOpen(true);
			nextPopupItemIndex_ = itemIndex;
			Application::Get()->Parent(StatefulWidget::New(nextPopup_.get()));
		}
	}

	void CloseSubmenu() {
		if(nextPopup_) {
			auto& item = items_[nextPopupItemIndex_];
			item->As<PopupSubmenuItem>()->SetOpen(false);
			nextPopup_.reset();
			nextPopupItemIndex_ = 0;
		}
	}

	// Update position here because for now this functionality is used only by Popup
	void PositionPopup(LayoutNotification& notification) {
		auto  viewportSize = Application::Get()->GetState().windowSize;
		auto* target = notification.source;
		auto  targetRectGlobal = target->GetRectGlobal();
		Point finalPos = targetRectGlobal.min;

		if(previousPopup_) {
			auto* menuButton = previousPopup_->GetWidget()->FindChildOfClass<LayoutWidget>();
			auto  menuButtonRectGlobal = menuButton->GetRectGlobal();

			if(targetRectGlobal.Right() > viewportSize.x) {
				finalPos.x = menuButtonRectGlobal.TL().x - targetRectGlobal.Width();
				finalPos.x = math::Clamp(finalPos.x, 0.f);
			}
		} else {
			if(targetRectGlobal.Right() > viewportSize.x) {
				finalPos.x = viewportSize.x - targetRectGlobal.Width();
				finalPos.x = math::Clamp(finalPos.x, 0.f);
			}
		}
		if(targetRectGlobal.Bottom() > viewportSize.y) {
			finalPos.y = viewportSize.y - targetRectGlobal.Height();
			finalPos.y = math::Clamp(finalPos.y, 0.f);
		}
		target->SetOrigin(finalPos);
	}

private:
	TimerHandle  				timerHandle_{};
	Point 					    pos_;
	// Either a PopupPortal or another Popup is our owner
	PopupPortal*                portal_ = nullptr;
	PopupState*                 previousPopup_ = nullptr;
	std::unique_ptr<PopupState> nextPopup_;
	u32							nextPopupItemIndex_ = 0;
	// Items provided by the user
	std::vector<std::unique_ptr<WidgetState>> items_;
};


/*
 * Creates a user defined popup when clicked or hovered
 */
class PopupPortal: public MouseRegion {
	WIDGET_CLASS(PopupPortal, MouseRegion)
public:

	struct StateEnum {
		static inline StringID Normal{"Normal"};
		static inline StringID Opened{"Opened"};
	};

	enum class SpawnEventType {
		LeftMouseRelease,
		RightMouseRelease,
		Hover,
	};

public:

	static auto New(const PopupBuilderFunc&	builder, std::unique_ptr<Widget>&& child) {
		return std::make_unique<PopupPortal>(builder, std::move(child));
	}

	PopupPortal(const PopupBuilderFunc&	builder, std::unique_ptr<Widget>&& child)
		: MouseRegion(MouseRegionConfig{
			.onMouseButton = [this](const auto& e) { OnMouseButton(e); },
			.bHandleHoverAlways = true,
		})
		, builder_(builder) {
		Parent(std::move(child));
	}
	
	void OnMouseButton(const MouseButtonEvent& event) {
		if(event.button == MouseButton::ButtonRight && !event.bPressed) {
			OpenPopup();
		}
	}
	
	// Can be called by the user to create a popup
	void OpenPopup() {
		if(!popup_) {
			auto pos = Application::Get()->GetState().mousePosGlobal;
			popup_ = std::make_unique<PopupState>(builder_, this, pos);
			Application::Get()->Parent(StatefulWidget::New(popup_.get()));
		}
	}

	void ClosePopup() {
		if(popup_) {
			popup_.reset();
		}
	}

	bool IsOpened() const { return !popup_; }

private:
	// The state is owned by us but the stateful widgets that uses this state
	// is owned by the Application
	std::unique_ptr<PopupState> popup_;
	PopupBuilderFunc            builder_;
};


}  // namespace UI


inline std::unique_ptr<UI::Widget> UI::PopupMenuItem::Build() {
	// TODO: use Container instead of Button and change the style on rebuild
	return MouseRegion::Build()
		.OnMouseEnter([this]() { parent_->OnItemHovered(index_, true); })
		.OnMouseLeave([this]() { parent_->OnItemHovered(index_, false); })
		.HandleHoverAlways()
		.Child(Button::Build()
			.StyleClass("PopupMenuItem") 
			.SizeMode({AxisMode::Expand, AxisMode::Shrink})
			.OnPress([this](const ButtonEvent& e) {
				if(e.button == MouseButton::ButtonLeft && !e.bPressed) {
					this->onPress_();
					this->parent_->OnItemPressed(index_); 
				}
			})
			.Child(Flexbox::Build()
				.DirectionRow()
				.Children(
					Flexible::New(7, 
						Aligned::New(Alignment::Start, Alignment::Start, 
							TextBox::New(text_)
						)
					),
					Flexible::New(3, 
						Aligned::New(Alignment::End, Alignment::Start, 
							TextBox::New(shortcut_)
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
		.OnMouseEnter([this]() { 
			parent_->OnItemHovered(index_, true); 
			state_ = StateEnum::Hovered; 
			//MarkNeedsRebuild();
		})
		.OnMouseLeave([this]() { 
			parent_->OnItemHovered(index_, false); 
			state_ = StateEnum::Normal; 
			//MarkNeedsRebuild();
		})
		.OnMouseButton([this](const MouseButtonEvent& e) { 
			if(e.button == MouseButton::ButtonLeft && !e.bPressed) {
				this->parent_->OnItemPressed(index_); 
			}
		})
		.Child(Container::Build()
			.StyleClass("PopupMenuItem") 
			.BoxStyle(isOpen_ ? StateEnum::Opened : state_)
			.SizeMode({AxisMode::Expand, AxisMode::Shrink})
			.Child(Flexbox::Build()
				.DirectionRow()
				.Children(
					Flexible::New(7, 
						Aligned::New(Alignment::Start, Alignment::Start, 
							TextBox::New(text_)
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
	if(portal_) {
		portal_->ClosePopup(); 
	} else if(previousPopup_) {
		previousPopup_->Close();
	} else {
		Assertm(false, "PopupState has no owner.");
	}
}