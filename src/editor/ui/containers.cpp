#include "containers.h"

namespace ui {

std::unique_ptr<Flexbox> FlexboxBuilder::New() {
    auto out = std::make_unique<Flexbox>();
    out->SetID(id);
    out->SetLayoutStyle(
        Application::Get()->GetTheme()->Find(style)->Find<LayoutStyle>());

    const Axis mainAxis =
        direction == ContentDirection::Row ? Axis::X : Axis::Y;
    out->SetAxisMode(mainAxis,
                     expandMainAxis ? AxisMode::Expand : AxisMode::Shrink);
    out->SetAxisMode(FlipAxis(mainAxis),
                     expandCrossAxis ? AxisMode::Expand : AxisMode::Shrink);

    out->direction_ = direction;
    out->justifyContent_ = justifyContent;
    out->alignment_ = alignment;
    out->overflowPolicy_ = overflowPolicy;

    for (auto& child : children) {
        out->Parent(std::move(child));
    }
    return out;
}

void Flexbox::DebugSerialize(PropertyArchive& ar) {
    MultiChildLayoutWidget::DebugSerialize(ar);
    ar.PushProperty("Direction", direction_);
    ar.PushProperty("JustifyContent", justifyContent_);
    ar.PushProperty("Alignment", alignment_);
    ar.PushProperty("OverflowPolicy", overflowPolicy_);
}

float2 Flexbox::OnLayout(const LayoutConstraints& event) {
    MultiChildLayoutWidget::OnLayout(event);
    LayOutChildren(event);
    return GetOuterSize();
}

struct TempChildData {
    LayoutWidget* child = nullptr;
    bool isFlexible = true;
    float crossAxisPos = 0;
    float mainAxisPos = 0;
    float crossAxisSize = 0;
    // Negative for flexible, positive for fixed
    float mainAxisSize = 0;
};

void Flexbox::LayOutChildren(const LayoutConstraints& event) {
    const SizeMode axisMode = GetAxisMode();
    const bool bDirectionRow = direction_ == ContentDirection::Row;
    const Axis mainAxisIndex = bDirectionRow ? Axis::X : Axis::Y;
    const Axis crossAxisIndex = bDirectionRow ? Axis::Y : Axis::X;
    const Paddings paddings = GetLayoutStyle()->paddings;
    const Vec2f innerSize = event.rect.Size() - paddings.Size();
    const float innerMainAxisSize = innerSize[mainAxisIndex];
    const float innerCrossAxisSize = innerSize[crossAxisIndex];
    // These options require extra space left on the main axis to work so we
    // need to find it
    bool bJustifyContent = justifyContent_ != JustifyContent::Start;
    std::vector<TempChildData> childrenData;

    float totalFlexFactor = 0;
    // Widgets which size doesn't depend on our size
    float fixedChildrenSizeMainAxis = 0;

    // #1 collect flexible info and update not expanded children
    VisitChildren([&](Widget* child) {
        bool hasFlexible = false;
        float flexFactor = 0.f;
        LayoutWidget* layoutChild = LayoutWidget::FindNearest(child);

        if (!layoutChild || !layoutChild->IsVisible()) {
            return VisitResult::Continue();
        }
        // Check if layout is wrapped in a Flexible
        for(Widget* ancestor: IterateAncestors(layoutChild)) {
            if (ancestor == this) {
                break;
            }
            if (auto* flexible = ancestor->As<Flexible>()) {
                flexFactor = flexible->GetFlexFactor();
                hasFlexible = true;
                break;
            }
        }

        TempChildData& childData = childrenData.emplace_back();
        childData.child = layoutChild;

        if (hasFlexible) {
            childData.mainAxisSize = -flexFactor;
            totalFlexFactor += childData.mainAxisSize;
            DASSERT_F(axisMode[mainAxisIndex] != AxisMode::Shrink,
                      "{} main axis set to AxisMode::Shrink, but the child {} "
                      "has axis set to AxisMode::Expand.",
                      GetDebugID(), layoutChild->GetDebugID());

        } else {
            const AxisMode mainAxisMode = layoutChild->GetAxisMode()[mainAxisIndex];

            if (mainAxisMode == AxisMode::Expand) {
                DASSERT_F(axisMode[mainAxisIndex] != AxisMode::Shrink,
                          "{} main axis set to AxisMode::Shrink, but the child "
                          "{} has axis set to AxisMode::Expand.",
                          GetDebugID(), layoutChild->GetDebugID());
                childData.mainAxisSize = -1.f;
                totalFlexFactor += childData.mainAxisSize;
            } else {
                LayoutConstraints e;
                e.rect = Rect(paddings.TL(), innerSize);
                const Vec2f size = layoutChild->OnLayout(e);

                childData.mainAxisSize = size[mainAxisIndex];
                childData.crossAxisSize = size[crossAxisIndex];
                childData.isFlexible = false;
                fixedChildrenSizeMainAxis += childData.mainAxisSize;
            }
        }
        // Cross axis
        const AxisMode crossAxisMode = layoutChild->GetAxisMode()[crossAxisIndex];

        if (crossAxisMode == AxisMode::Expand) {
            DASSERT_F(axisMode[crossAxisIndex] != AxisMode::Shrink,
                      "{} main axis set to AxisMode::Shrink, but the child {} "
                      "has axis set to AxisMode::Expand.",
                      GetDebugID(), layoutChild->GetDebugID());
            childData.crossAxisSize = innerCrossAxisSize;
        }
        return VisitResult::Continue();
    });
    float mainAxisFlexibleSpace =
        math::Clamp(innerMainAxisSize - fixedChildrenSizeMainAxis, 0.f);

    // Check for overflow
    // TODO: use min size from theme here
    if (axisMode[mainAxisIndex] == AxisMode::Expand &&
        mainAxisFlexibleSpace <= 0.f) {
        if (overflowPolicy_ == OverflowPolicy::Wrap) {
            // Check if can expand in cross axis and put children there
        } else if (overflowPolicy_ == OverflowPolicy::ShrinkWrap) {
            // Do not decrease our size below content size
        }
    }

    // #2 Calculate sizes for flexible widgets if possible
    if (totalFlexFactor < 0.f) {
        bJustifyContent = false;

        for (auto& childData : childrenData) {
            if (childData.mainAxisSize < 0.f) {
                childData.mainAxisSize = mainAxisFlexibleSpace *
                                         childData.mainAxisSize /
                                         totalFlexFactor;
                /// TODO Child size shouldn't be less then Child.GetMinSize()
                childData.mainAxisSize =
                    math::Clamp(childData.mainAxisSize, 0.f);
            }
        }
    }

    // #3 Calculate the space divisions for content justification
    // This is the margin added to items
    float justifyContentMargin = 0.f;

    if (bJustifyContent && axisMode[mainAxisIndex] == AxisMode::Expand) {
        switch (justifyContent_) {
            case JustifyContent::End: {
                justifyContentMargin = mainAxisFlexibleSpace;
                break;
            }
            case JustifyContent::Center: {
                justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
                break;
            }
            case JustifyContent::SpaceBetween: {
                if (childrenData.size() == 1) {
                    justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
                } else {
                    justifyContentMargin =
                        mainAxisFlexibleSpace / (childrenData.size() - 1);
                }
                break;
            }
            case JustifyContent::SpaceAround: {
                if (childrenData.size() == 1) {
                    justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
                } else {
                    justifyContentMargin =
                        mainAxisFlexibleSpace / (childrenData.size() + 1);
                }
                break;
            }
        }
    }

    // If our size depends on children, set this to max temporarily
    float crossAxisSize = axisMode[crossAxisIndex] == AxisMode::Expand
                             ? innerSize[crossAxisIndex]
                             : std::numeric_limits<float>::max();
    float mainAxisCursor = (float)paddings.TL()[mainAxisIndex];

    if (justifyContent_ == JustifyContent::Center ||
        justifyContent_ == JustifyContent::End ||
        justifyContent_ == JustifyContent::SpaceAround) {
        mainAxisCursor = justifyContentMargin;
    }
    float maxChildSizeCrossAxis = 0.f;

    // #4 At this point we have shrink shildren already updated
    // and main axis flexibles calculated
    // So we need to update the flexibles and align all widgets later
    for (TempChildData& childData : childrenData) {
        // Cache position for later
        childData.mainAxisPos = mainAxisCursor;

        Vec2f constraints;
        constraints[mainAxisIndex] = childData.mainAxisSize;
        constraints[crossAxisIndex] = crossAxisSize;
        LayoutConstraints layoutEvent;
        layoutEvent.parent = this;
        layoutEvent.rect = Rect(Vec2f(), constraints);

        Vec2f widgetSize;

        if (childData.isFlexible) {
            widgetSize = childData.child->OnLayout(layoutEvent);
            childData.crossAxisSize = widgetSize[crossAxisIndex];
        } else {
            // Calculated at #1
            widgetSize = childData.child->GetOuterSize();
        }
        mainAxisCursor += constraints[mainAxisIndex];
        maxChildSizeCrossAxis =
            math::Max(maxChildSizeCrossAxis, widgetSize[crossAxisIndex]);

        if (bJustifyContent &&
                justifyContent_ == JustifyContent::SpaceBetween ||
            justifyContent_ == JustifyContent::SpaceAround) {
            mainAxisCursor += justifyContentMargin;
        }
    }

    // #5 Update our size if it depends on children
    const float totalMainAxisContentSize = mainAxisCursor;
    if (axisMode[crossAxisIndex] == AxisMode::Shrink) {
        crossAxisSize = maxChildSizeCrossAxis;
        SetSize(crossAxisIndex, crossAxisSize);
    }
    float mainAxisSize = GetSize()[mainAxisIndex];
    if (axisMode[mainAxisIndex] == AxisMode::Shrink) {
        mainAxisSize = fixedChildrenSizeMainAxis;
    }
    if (axisMode[mainAxisIndex] == AxisMode::Shrink ||
        (overflowPolicy_ == OverflowPolicy::ShrinkWrap &&
         totalMainAxisContentSize > innerMainAxisSize)) {
        SetSize(mainAxisIndex, totalMainAxisContentSize);
    }

    // #6 At this point all sizes are calculated and we can update children
    // positions
    for (TempChildData& childData : childrenData) {
        Vec2f pos;
        pos[mainAxisIndex] = childData.mainAxisPos;

        if (alignment_ == Alignment::End) {
            pos[crossAxisIndex] = crossAxisSize - childData.crossAxisSize;
        } else if (alignment_ == Alignment::Center) {
            pos[crossAxisIndex] =
                0.5f * (crossAxisSize - childData.crossAxisSize);
        }
        const Sides margins = childData.child->GetLayoutStyle()->margins;
        childData.child->SetOrigin(pos + margins.TL());
        childData.child->OnPostLayout();
    }
}

}  // namespace ui