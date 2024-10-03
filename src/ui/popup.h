#pragma once
#include "widgets.h"

namespace ui {


class PopupPortal;
class PopupBuilder;
class PopupState;

// TODO: maybe remove
struct PopupOpenContext {
    Point mousePosGlobal;
    // Current state that builds the popup
    PopupState* current = nullptr;
};

using PopupMenuItemOnPressFunc = std::function<void()>;

// Created by the user inside the builder callback
// Owned by PopupState
class PopupMenuItem : public WidgetState {
    WIDGET_CLASS(PopupMenuItem, WidgetState)
public:
    PopupMenuItem(const std::string& text,
                  const std::string& shortcut,
                  const PopupMenuItemOnPressFunc& onPress)
        : text_(text)
        , shortcut_(shortcut)
        , onPress_(onPress)
        , parent_(nullptr)
        , index_(0) {}

    void Bind(PopupState* parent, uint32_t index) {
        parent_ = parent;
        index_ = index;
    }
    std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) override;

private:
    std::string text_;
    std::string shortcut_;
    PopupMenuItemOnPressFunc onPress_;
    PopupState* parent_;
    uint32_t index_;
};

// Opens a submenu
class PopupSubmenuItem : public WidgetState {
    WIDGET_CLASS(PopupSubmenuItem, WidgetState)
public:
    PopupSubmenuItem(const std::string& text, const PopupBuilderFunc& builder)
        : text_(text)
        , isOpen_(false)
        , builder_(builder)
        , parent_(nullptr)
        , index_(0)
        , state_(StateEnum::Normal) {}

    void Bind(PopupState* parent, uint32_t index) {
        parent_ = parent;
        index_ = index;
    }
    std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) override;
    const PopupBuilderFunc& GetBuilder() const { return builder_; }
    void SetOpen(bool isOpen) {
        isOpen_ = isOpen;
        MarkNeedsRebuild();
    }

private:
    StringID state_;
    bool isOpen_;
    std::string text_;
    PopupState* parent_;
    uint32_t index_;
    PopupBuilderFunc builder_;
};

/*
 * TODO: Add collisions and positioning. Mayve use LayouNotifications and
 * ScreenCollider that uses callbacks Contains and builds a list of user
 * provided menu items in the builder function Owner is a PopupPortal or another
 * PopupState of the previous popup
 */
class PopupState : public WidgetState {
    WIDGET_CLASS(PopupState, WidgetState)
public:
    PopupState(const PopupBuilderFunc& builder, PopupPortal* owner, Point pos)
        : portal_(owner), pos_(pos) {
        items_ = builder(PopupOpenContext{});
        uint32_t index = 0;

        for (auto& item : items_) {
            if (auto* popupItem = item->As<PopupMenuItem>()) {
                popupItem->Bind(this, index);
            } else if (auto* submenuItem = item->As<PopupSubmenuItem>()) {
                submenuItem->Bind(this, index);
            } else {
                DASSERT_M(false,
                          "Popup menu content items should be of class "
                          "PopupMenuItem or PopupSubmenuItem");
            }
            ++index;
        }
    }

    PopupState(const PopupBuilderFunc& builder, PopupState* owner, Point pos)
        : PopupState(builder, (PopupPortal*)nullptr, pos) {
        previousPopup_ = owner;
    }

    ~PopupState() {
        if (timerHandle_) {
            Application::Get()->RemoveTimer(timerHandle_);
        }
    }

    std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) override {
        std::vector<std::unique_ptr<Widget>> out;
        for (auto& item : items_) {
            out.push_back(StatefulWidget::New(item.get()));
        }
        auto body = EventListener::New(
            [this](IEvent* event) {
                auto* e = event->As<LayoutNotification>();
                if (e && e->depth == 0) {
                    this->PositionPopup(*e);
                    return true;
                }
                return false;
            },
            Container::Build()
                .PositionFloat(pos_)
                .NotifyOnLayoutUpdate()
                .SizeMode(AxisMode::Fixed, AxisMode::Shrink)
                .Size(float2{300, 0})
                .ID("PopupBody")
                .StyleClass("Popup")
                .Child(Flexbox::Build()
                           .DirectionColumn()
                           .ExpandCrossAxis(true)
                           .ExpandMainAxis(false)
                           .Children(std::move(out))
                           .New())
                .New());
        if (previousPopup_) {
            return body;
        } else {
            // Container with the size of the screen to capture all mouse events
            return MouseRegion::Build()
                .OnMouseButton([this](const auto&) { Close(); })
                .Child(Container::Build()
                           .ID("PopupScreenCapture")
                           .StyleClass("Transparent")
                           .SizeMode(SizeMode::Expand())
                           .Child(std::move(body))
                           .New())
                .New();
        }
    }

    void OnItemHovered(uint32_t itemIndex, bool bEnter) {
        constexpr auto timerDelayMs = 500;
        if (bEnter) {
            auto timerCallback = [this, index = itemIndex]() -> bool {
                DASSERT(index < items_.size());
                if (auto* item = items_[index]->As<PopupSubmenuItem>()) {
                    OpenSubmenu(*item, index);
                } else {
                    CloseSubmenu();
                }
                return false;
            };
            timerHandle_ =
                Application::Get()->AddTimer(this, timerCallback, timerDelayMs);
        } else {
            Application::Get()->RemoveTimer(timerHandle_);
        }
    }

    void OnItemPressed(uint32_t itemIndex) {
        DASSERT(itemIndex < items_.size());
        if (auto* item = items_[itemIndex]->As<PopupSubmenuItem>()) {
            OpenSubmenu(*item, itemIndex);
        } else {
            Close();
        }
    }

    void Close();

private:
    void OpenSubmenu(PopupSubmenuItem& item, uint32_t itemIndex) {
        if (nextPopup_ && nextPopupItemIndex_ != itemIndex) {
            CloseSubmenu();
        }
        if (!nextPopup_) {
            auto* layoutWidget =
                item.GetWidget()->FindChildOfClass<LayoutWidget>();
            auto rectGlobal = layoutWidget->GetRectGlobal();
            nextPopup_ = std::make_unique<PopupState>(item.GetBuilder(), this,
                                                      rectGlobal.TR());
            item.SetOpen(true);
            nextPopupItemIndex_ = itemIndex;
            Application::Get()->Parent(StatefulWidget::New(nextPopup_.get()));
        }
    }

    void CloseSubmenu() {
        if (nextPopup_) {
            auto& item = items_[nextPopupItemIndex_];
            item->As<PopupSubmenuItem>()->SetOpen(false);
            nextPopup_.reset();
            nextPopupItemIndex_ = 0;
        }
    }

    // Update position here because for now this functionality is used only by
    // Popup
    void PositionPopup(LayoutNotification& notification) {
        LayoutWidget* target = notification.source;
        const Vec2f viewportSize = Application::Get()->GetState().windowSize;
        const Rect targetRectGlobal = target->GetRectGlobal();
        Point finalPos = targetRectGlobal.min;

        if (previousPopup_) {
            auto* menuButton =
                previousPopup_->GetWidget()->FindChildOfClass<LayoutWidget>();
            auto menuButtonRectGlobal = menuButton->GetRectGlobal();

            if (targetRectGlobal.Right() > viewportSize.x) {
                finalPos.x =
                    menuButtonRectGlobal.TL().x - targetRectGlobal.Width();
                finalPos.x = math::Clamp(finalPos.x, 0.f);
            }
        } else {
            if (targetRectGlobal.Right() > viewportSize.x) {
                finalPos.x = viewportSize.x - targetRectGlobal.Width();
                finalPos.x = math::Clamp(finalPos.x, 0.f);
            }
        }
        if (targetRectGlobal.Bottom() > viewportSize.y) {
            finalPos.y = viewportSize.y - targetRectGlobal.Height();
            finalPos.y = math::Clamp(finalPos.y, 0.f);
        }
        target->SetOrigin(finalPos);
    }

private:
    TimerHandle timerHandle_{};
    Point pos_;
    // Either a PopupPortal or another Popup is our owner
    PopupPortal* portal_ = nullptr;
    PopupState* previousPopup_ = nullptr;
    std::unique_ptr<PopupState> nextPopup_;
    uint32_t nextPopupItemIndex_ = 0;
    // Items provided by the user
    std::vector<std::unique_ptr<WidgetState>> items_;
};


/*
 * Creates a user defined popup when clicked or hovered
 */
class PopupPortal : public MouseRegion {
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
    static auto New(const PopupBuilderFunc& builder,
                    std::unique_ptr<Widget>&& child) {
        return std::make_unique<PopupPortal>(builder, std::move(child));
    }

    PopupPortal(const PopupBuilderFunc& builder,
                std::unique_ptr<Widget>&& child)
        : MouseRegion(MouseRegionConfig{
              .onMouseButton = [this](const auto& e) { OnMouseButton(e); },
              .bHandleHoverAlways = true,
          })
        , builder_(builder) {
        Parent(std::move(child));
    }

    bool UpdateWith(const Widget* newWidget) override {
        if (auto* w = newWidget->As<PopupPortal>()) {
            builder_ = w->builder_;
            return true;
        }
        return false;
    }

    void OnMouseButton(const MouseButtonEvent& event) {
        if (event.button == MouseButton::ButtonRight && !event.isPressed) {
            OpenPopup();
        }
    }

    // Can be called by the user to create a popup
    void OpenPopup() {
        if (!popup_) {
            auto pos = Application::Get()->GetState().mousePosGlobal;
            popup_ = std::make_unique<PopupState>(builder_, this, pos);
            Application::Get()->Parent(StatefulWidget::New(popup_.get()));
        }
    }

    void ClosePopup() {
        if (popup_) {
            popup_.reset();
        }
    }

    bool IsOpened() const { return !popup_; }

private:
    // The state is owned by us but the stateful widgets that uses this state
    // is owned by the Application
    std::unique_ptr<PopupState> popup_;
    PopupBuilderFunc builder_;
};

}  // namespace ui


inline std::unique_ptr<ui::Widget> ui::PopupMenuItem::Build(
    std::unique_ptr<Widget>&&) {
    // TODO: use Container instead of Button and change the style on rebuild
    return MouseRegion::Build()
        .OnMouseEnter([this]() { parent_->OnItemHovered(index_, true); })
        .OnMouseLeave([this]() { parent_->OnItemHovered(index_, false); })
        .HandleHoverAlways()
        .Child(Button::Build()
                   .StyleClass("PopupMenuItem")
                   .SizeMode(AxisMode::Expand, AxisMode::Shrink)
                   .OnPress([this](const ButtonEvent& e) {
                       if (e.button == MouseButton::ButtonLeft && !e.bPressed) {
                           this->onPress_();
                           this->parent_->OnItemPressed(index_);
                       }
                   })
                   .Child(Flexbox::Build()
                              .DirectionRow()
                              .Children(
                                  Flexible::New(
                                      7, Aligned::New(Alignment::Start,
                                                      Alignment::Start,
                                                      TextBox::New(text_))),
                                  Flexible::New(
                                      3, Aligned::New(Alignment::End,
                                                      Alignment::Start,
                                                      TextBox::New(shortcut_))))
                              .New())
                   .New())
        .New();
}

inline std::unique_ptr<ui::Widget> ui::PopupSubmenuItem::Build(
    std::unique_ptr<Widget>&&) {
    return MouseRegion::Build()
        .OnMouseEnter([this]() {
            parent_->OnItemHovered(index_, true);
            state_ = StateEnum::Hovered;
            MarkNeedsRebuild();
        })
        .OnMouseLeave([this]() {
            parent_->OnItemHovered(index_, false);
            state_ = StateEnum::Normal;
            MarkNeedsRebuild();
        })
        .OnMouseButton([this](const MouseButtonEvent& e) {
            if (e.button == MouseButton::ButtonLeft && !e.isPressed) {
                this->parent_->OnItemPressed(index_);
            }
        })
        .Child(
            Container::Build()
                .StyleClass("PopupMenuItem")
                .BoxStyle(isOpen_ ? StateEnum::Opened : state_)
                .SizeMode(AxisMode::Expand, AxisMode::Shrink)
                .Child(
                    Flexbox::Build()
                        .DirectionRow()
                        .Children(
                            Flexible::New(7, Aligned::New(Alignment::Start,
                                                          Alignment::Start,
                                                          TextBox::New(text_))),
                            Flexible::New(3, Aligned::New(Alignment::End,
                                                          Alignment::Start,
                                                          TextBox::New(">"))))
                        .New())
                .New())
        .New();
}

inline void ui::PopupState::Close() {
    if (portal_) {
        portal_->ClosePopup();
    } else if (previousPopup_) {
        previousPopup_->Close();
    } else {
        DASSERT_M(false, "PopupState has no owner.");
    }
}