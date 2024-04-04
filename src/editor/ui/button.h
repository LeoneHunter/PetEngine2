#pragma once
#include "widgets.h"
#include "tooltip.h"
#include "containers.h"

namespace UI {

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
		archive.PushProperty("State", *(StringID)state_);
	}

	bool UpdateWith(const Widget* newWidget) override {
		if(auto* w = newWidget->As<Button>()) {
			CopyConfiguration(*w);
			return true;
		}
		return false;
	}

protected:
	Button(const ButtonEventFunc& eventCallback)
		: state_(StateEnum::Normal)
		, eventCallback_(eventCallback) 
	{}
	
	void CopyConfiguration(const Button& other) {
		Super::CopyConfiguration(other);
		eventCallback_ = other.eventCallback_;
	}

	friend class ButtonBuilder;

private:
	StringID        state_;
	ButtonEventFunc eventCallback_;
};

class ButtonBuilder {
public:
	auto& ID(StringID inID) { container.ID(inID); return *this; }
	auto& SizeFixed(float2 size) { container.SizeFixed(size); return *this; }	
	auto& SizeExpanded(bool bHorizontal = true, bool bVertical = true) { container.SizeExpanded(); return *this; }	
	auto& SizeMode(AxisMode mode) { container.SizeMode(mode); return *this;  }
	auto& PositionFloat(Point inPos) { container.PositionFloat(inPos); return *this; }
	auto& StyleClass(StringID styleName) { styleClass = styleName; return *this; }
	auto& ClipContent(bool bClip) { container.ClipContent(bClip); return *this; }
	auto& AlignContent(Alignment x, Alignment y) { alignX = x; alignY = y; return *this; }
	auto& Text(const std::string& text) { this->text = text; return *this; }
	auto& Tooltip(const std::string& text) { tooltipText = text; return *this; }
	auto& OnPress(const ButtonEventFunc& callback) { this->callback = callback; return *this; }
	auto& Child(std::unique_ptr<Widget>&& child) { this->child = std::move(child); return *this; }

	std::unique_ptr<Button> New() {
        auto child = this->child 
                ? std::move(this->child)
                : std::make_unique<TextBox>(text);

        if(alignX != Alignment::Start || alignY != Alignment::Start) {
            child = Aligned::New(alignX, alignY, std::move(child));
        }
        child = container
			.StyleClass(styleClass)
			.Child(std::move(child))
			.New();

        if(!tooltipText.empty()) {
            child = TooltipPortal::New(
                [text = tooltipText](const TooltipBuildContext& ctx) { 
                    return TextTooltipBuilder()
                        .Text(text)
                        .New();
                },
                std::move(child)
            );
        }
        std::unique_ptr<Button> out(new Button(callback));
        out->Parent(std::move(child));
        return out;
    } 

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
}