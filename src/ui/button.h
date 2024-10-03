#pragma once
#include "containers.h"
#include "tooltip.h"
#include "widgets.h"

namespace ui {

class Button;
class ButtonBuilder;

class ButtonEvent : public IEvent {
    EVENT_CLASS(ButtonEvent, IEvent);

public:
    ButtonEvent(Button* source, MouseButton btn, bool bPressed)
        : source(source), button(btn), bPressed(bPressed) {}

    EventCategory GetCategory() const override {
        return EventCategory::Callback;
    }

    Button* source;
    MouseButton button;
    bool bPressed;
};

using ButtonEventFunc = std::function<void(const ButtonEvent&)>;

/*
 * Simple clickable button with flat style
 * TODO: maybe use composited widgets from MouseRegion and Container instead
 * and use some bindings or notifications
 */
class Button : public SingleChildWidget {
    WIDGET_CLASS(Button, SingleChildWidget)
public:
    static ButtonBuilder Build();

    bool OnEvent(IEvent* event) override {
        if (auto* hoverEvent = event->As<HoverEvent>()) {
            // Change state and style
            if (hoverEvent->bHoverEnter) {
                state_ = StateEnum::Hovered;
                FindChildOfClass<Container>()->SetBoxStyleName(
                    StateEnum::Hovered);

            } else if (hoverEvent->bHoverLeave) {
                state_ = StateEnum::Normal;
                FindChildOfClass<Container>()->SetBoxStyleName(
                    StateEnum::Normal);
            }
            return true;
        }
        if (auto* mouseButtonEvent = event->As<MouseButtonEvent>()) {
            if (mouseButtonEvent->button == MouseButton::ButtonLeft) {
                if (mouseButtonEvent->isPressed) {
                    state_ = StateEnum::Pressed;
                    FindChildOfClass<Container>()->SetBoxStyleName(
                        StateEnum::Pressed);

                } else {
                    auto* container = FindChildOfClass<Container>();

                    if (container->GetRect().Contains(
                            mouseButtonEvent->mousePosLocal)) {
                        state_ = StateEnum::Hovered;
                        container->SetBoxStyleName(StateEnum::Hovered);
                    } else {
                        state_ = StateEnum::Normal;
                        container->SetBoxStyleName(StateEnum::Normal);
                    }
                }
                if (eventCallback_) {
                    ButtonEvent btnEvent(this, mouseButtonEvent->button,
                                         mouseButtonEvent->isPressed);
                    eventCallback_(btnEvent);
                }
                return true;
            }
            return false;
        }
        return SingleChildWidget::OnEvent(event);
    }

    void DebugSerialize(PropertyArchive& archive) override {
        SingleChildWidget::DebugSerialize(archive);
        archive.PushProperty("State", *(StringID)state_);
    }

protected:
    Button(const ButtonEventFunc& eventCallback)
        : state_(StateEnum::Normal), eventCallback_(eventCallback) {}

    void CopyConfiguration(const Button& other) {
        SingleChildWidget::CopyConfiguration(other);
        eventCallback_ = other.eventCallback_;
    }

    friend class ButtonBuilder;

private:
    StringID state_;
    ButtonEventFunc eventCallback_;
};

class ButtonBuilder {
public:
    auto& ID(StringID inID) {
        container_.ID(inID);
        return *this;
    }
    auto& SizeFixed(float2 size) {
        container_.SizeFixed(size);
        return *this;
    }
    auto& SizeMode(SizeMode mode) {
        container_.SizeMode(mode);
        return *this;
    }
    auto& SizeMode(AxisMode x, AxisMode y) {
        container_.SizeMode(x, y);
        return *this;
    }
    auto& PositionFloat(Point inPos) {
        container_.PositionFloat(inPos);
        return *this;
    }
    auto& StyleClass(StringID styleName) {
        styleClass_ = styleName;
        return *this;
    }
    auto& ClipContent(bool bClip) {
        container_.ClipContent(bClip);
        return *this;
    }
    auto& AlignContent(Alignment x, Alignment y) {
        alignX_ = x;
        alignY_ = y;
        return *this;
    }
    auto& Text(const std::string& text) {
        text_ = text;
        return *this;
    }
    auto& Tooltip(const std::string& text) {
        tooltipText_ = text;
        return *this;
    }
    auto& OnPress(const ButtonEventFunc& callback) {
        callback_ = callback;
        return *this;
    }
    auto& Child(std::unique_ptr<Widget>&& child) {
        child_ = std::move(child);
        return *this;
    }

    std::unique_ptr<Button> New() {
        auto child = child_ ? std::move(this->child_)
                                 : std::make_unique<TextBox>(text_);

        if (alignX_ != Alignment::Start || alignY_ != Alignment::Start) {
            child = Aligned::New(alignX_, alignY_, std::move(child));
        }
        child = container_.StyleClass(styleClass_).Child(std::move(child)).New();

        if (!tooltipText_.empty()) {
            child = TooltipPortal::New(
                [text = tooltipText_](const TooltipBuildContext& ctx) {
                    return TextTooltipBuilder().Text(text).New();
                },
                std::move(child));
        }
        std::unique_ptr<Button> out(new Button(callback_));
        out->Parent(std::move(child));
        return out;
    }

private:
    Alignment alignX_ = Alignment::Start;
    Alignment alignY_ = Alignment::Start;
    ButtonEventFunc callback_;
    ContainerBuilder container_;
    std::string text_;
    std::string tooltipText_;
    StringID styleClass_ = "Button";
    std::unique_ptr<Widget> child_;
};

inline ButtonBuilder Button::Build() {
    return {};
}
}  // namespace ui