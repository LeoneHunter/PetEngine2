#include "widgets.h"
#include "runtime/system/renderer/ui_renderer.h"
#include "ui.h"

namespace UI {

/*-------------------------------------------------------------------------------------------------*/
//										TEXT
/*-------------------------------------------------------------------------------------------------*/
UI::TextBox::TextBox(const std::string& inText, StringID inStyle)
	: Super(Application::Get()->GetTheme()->Find(inStyle)->Find<LayoutStyle>()) 
	, m_Style(Application::Get()->GetTheme()->Find(inStyle))
	, m_Text(inText) {
	// const auto size = m_Style->Find<TextStyle>()->CalculateTextSize(m_Text);
	// SetSize(size);
	SetAxisMode(axisModeFixed);
}

void UI::TextBox::SetText(const std::string& inText) {
	m_Text = inText;
	float2 size;

	if(inText.empty()) {
		// TODO handle the case when the text is empty
		// Maybe do not update parent
		// For now we set it to some random values
		size = float2(15, 15);
	} else {
		// size = m_Style->Find<TextStyle>()->CalculateTextSize(m_Text);
		// SetSize(size);
	}
	// DispatchLayoutToParent();
}

bool UI::TextBox::OnEvent(IEvent* inEvent) {
	if(auto* event = inEvent->As<HoverEvent>()) {
		// Ignore, we cannot be hovered
		return false;
	}

	if(auto* event = inEvent->As<ParentLayoutEvent>()) {
		// Ignore size
		SetOrigin(event->constraints.TL());
		return false;
	}

	if(auto* event = inEvent->As<DrawEvent>()) {
		event->canvas->DrawText(GetOrigin(), m_Style->Find<TextStyle>(), m_Text);
		return true;
	}
	return Super::OnEvent(inEvent);
}

float2 TextBox::OnLayout(const ParentLayoutEvent& inEvent) {
	SetOrigin(inEvent.constraints.TL());
	const auto size = m_Style->Find<TextStyle>()->CalculateTextSize(m_Text);
	SetSize(size);
	const auto layoutInfo = *GetLayoutStyle();
	return GetSize() + layoutInfo.margins.Size();
}





std::unique_ptr<Button> ButtonBuilder::New() {
	auto child = this->child 
			? std::move(this->child)
			: std::make_unique<TextBox>(text);

	if(alignX != Alignment::Start || alignY != Alignment::Start) {
		child = Aligned::New(alignX, alignY, std::move(child));
	}
	child = container.StyleClass(styleClass).Child(std::move(child)).New();

	if(!tooltipText.empty()) {
		child = TooltipPortal::New(
			[text = tooltipText](const TooltipBuildContext& inCtx) { 
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


// Window* WindowBuilder::New() { 
// 	auto* root = new Window();

// 	auto* titleBar = MouseRegion::Build()
// 		.HandleButtonAlways(true)
// 		.OnMouseDrag([root](const MouseDragEvent& e) {
// 			root->Translate(e.mouseDelta);
// 		})
// 		.Child(Container::Build()
// 			.SizeMode({AxisMode::Expand, AxisMode::Shrink})
// 			.StyleClass("Titlebar")
// 			.Child(Flexbox::Build()
// 				.DirectionRow()
// 				.ID("TitleBarFlexbox")
// 				.Style("TitleBar")
// 				.JustifyContent(JustifyContent::SpaceBetween)
// 				.AlignCenter()
// 				.Children({
// 					new TextBox(titleText),
// 					Button::Build().StyleClass("CloseButton").Text("X").New()
// 				})
// 				.New())	
// 			.New())
// 		.New();

// 	auto content = Flexbox::Build()
// 			.ID("ContentArea")
// 			.DirectionColumn()
// 			.JustifyContent(JustifyContent::Start)
// 			.AlignStart()
// 			.Expand()
// 			.Children(std::move(children))
// 			.New();

// 	if(popupBuilder) {
// 		content = new PopupPortal(popupBuilder, content);
// 	}

// 	root->SetChild(Container::Build()
// 		.ClipContent(true)
// 		.SizeFixed(size)
// 		.StyleClass(style)
// 		.PositionFloat(pos)
// 		.Child(MouseRegion::Build()
// 			.HandleHoverAlways(true)
// 			.HandleButtonAlways(true)
// 			.OnMouseButton([root](const MouseButtonEvent& e) {
// 				if(e.bPressed) root->Focus();
// 			})
// 			.Child(Flexbox::Build()
// 				.DirectionColumn()
// 				.JustifyContent(JustifyContent::Start)
// 				.Alignment(Alignment::Center)
// 				.Expand()
// 				.Children({
// 					titleBar,
// 					content
// 				})
// 				.New())
// 			.New())
// 		.New()
// 	);
// 	return root;
// }


/*-------------------------------------------------------------------------------------------------*/
//										FLEXBOX
/*-------------------------------------------------------------------------------------------------*/
std::unique_ptr<Flexbox> UI::FlexboxBuilder::New() {
	auto out = std::make_unique<Flexbox>();
	out->SetID(id);
	out->SetLayoutStyle(Application::Get()->GetTheme()->Find(style)->FindOrDefault<LayoutStyle>());

	const auto mainAxis = direction == ContentDirection::Row ? Axis::X : Axis::Y;
	out->SetAxisMode(mainAxis, expandMainAxis ? AxisMode::Expand : AxisMode::Shrink);
	out->SetAxisMode(InvertAxis(mainAxis), expandCrossAxis ? AxisMode::Expand : AxisMode::Shrink);

	out->m_Direction = direction;
	out->m_JustifyContent = justifyContent;
	out->m_Alignment = alignment;
	out->m_OverflowPolicy = overflowPolicy;

	for(auto& child: children) {
		out->Parent(std::move(child));
	}
	return out;
}

bool UI::Flexbox::OnEvent(IEvent* inEvent) {
	if(auto* e = inEvent->As<ParentLayoutEvent>()) {
		// Update our size if expanded
		HandleParentLayoutEvent(e);
		// LayOutChildren(*e);
		return true;
	}
	if(auto* event = inEvent->As<ChildLayoutEvent>()) {
		// LayOutChildren(*event);
		DispatchLayoutToParent();
		return true;
	}
	return Super::OnEvent(inEvent);
}

void UI::Flexbox::DebugSerialize(PropertyArchive& ar) {
	Super::DebugSerialize(ar);
	ar.PushProperty("Direction", m_Direction);
	ar.PushProperty("JustifyContent", m_JustifyContent);
	ar.PushProperty("Alignment", m_Alignment);
	ar.PushProperty("OverflowPolicy", m_OverflowPolicy);
}

float2 Flexbox::OnLayout(const ParentLayoutEvent& inEvent) {
	Super::HandleParentLayoutEvent(&inEvent);
	LayOutChildren(inEvent);
	return GetOuterSize();
}

struct TempData {
	LayoutWidget* child         = nullptr;
	bool 		  isFlexible    = true;
	float         crossAxisPos  = 0;
	float         mainAxisPos   = 0;
	float         crossAxisSize = 0;
	// Negative for flexible, positive for fixed
	float 		  mainAxisSize  = 0;
};

void UI::Flexbox::LayOutChildren(const ParentLayoutEvent& inEvent) {
	const auto axisMode = GetAxisMode();
	const bool bDirectionRow = m_Direction == ContentDirection::Row;
	const auto mainAxisIndex = bDirectionRow ? Axis::X : Axis::Y;
	const auto crossAxisIndex = bDirectionRow ? Axis::Y : Axis::X;
	const auto paddings = GetLayoutStyle()->paddings;
	const auto innerSize = inEvent.constraints.Size() - paddings.Size();
	const auto innerMainAxisSize = innerSize[mainAxisIndex];
	const auto innerCrossAxisSize = innerSize[crossAxisIndex];
	// These options require extra space left on the main axis to work so we need to find it
	bool bJustifyContent = m_JustifyContent != JustifyContent::Start;
	std::vector<TempData> childrenData;

	float totalFlexFactor = 0;
	// Widgets which size doesn't depend on our size
	float fixedChildrenSizeMainAxis = 0;

	// #1 collect flexible info and update not expanded children
	VisitChildren([&](Widget* child) {
		auto flexFactor = 0.f;
		auto* layoutChild = LayoutWidget::FindNearest(child);
		
		if(!layoutChild || !layoutChild->IsVisible()) {
			return visitResultContinue;
		}
		// Check if layout is wrapped in a Flexible
		layoutChild->VisitParent([&](Widget* parent) {
			if(parent == this) {
				return visitResultExit;
			}
			if(auto* flexible = parent->As<Flexible>()) {
				flexFactor = flexible->GetFlexFactor();
				return visitResultExit;
			}
			return visitResultContinue;
		});
		auto& childData = childrenData.emplace_back();
		childData.child = layoutChild;

		if(flexFactor > 0.f) {
			childData.mainAxisSize = -flexFactor;
			totalFlexFactor += childData.mainAxisSize;
			flexFactor = 0.f;
			Assertf(axisMode[mainAxisIndex] != AxisMode::Shrink,
					"{} main axis set to AxisMode::Shrink, but the child {} has axis set to AxisMode::Expand.",
					GetDebugID(),
					layoutChild->GetDebugID());

		} else {
			const auto mainAxisMode = layoutChild->GetAxisMode()[mainAxisIndex];

			if(mainAxisMode == AxisMode::Expand) {
				Assertf(axisMode[mainAxisIndex] != AxisMode::Shrink,
						"{} main axis set to AxisMode::Shrink, but the child {} has axis set to AxisMode::Expand.",
						GetDebugID(),
						layoutChild->GetDebugID());
				childData.mainAxisSize = -1.f;
				totalFlexFactor += childData.mainAxisSize;
			} else {
				ParentLayoutEvent e;
				e.constraints = Rect(paddings.TL(), innerSize);
				const auto size = layoutChild->OnLayout(e);

				childData.mainAxisSize = size[mainAxisIndex];
				childData.crossAxisSize = size[crossAxisIndex];
				childData.isFlexible = false;
				fixedChildrenSizeMainAxis += childData.mainAxisSize;
			}
		}
		// Cross axis
		const auto crossAxisMode = layoutChild->GetAxisMode()[crossAxisIndex];

		if(crossAxisMode == AxisMode::Expand) {
			Assertf(axisMode[crossAxisIndex] != AxisMode::Shrink,
					"{} main axis set to AxisMode::Shrink, but the child {} has axis set to AxisMode::Expand.",
					GetDebugID(),
					layoutChild->GetDebugID());
			childData.crossAxisSize = innerCrossAxisSize;
		}
		return visitResultContinue;
	});
	float mainAxisFlexibleSpace = Math::Clamp(innerMainAxisSize - fixedChildrenSizeMainAxis, 0.f);

	// Check for overflow
	/// TODO use min size from theme here
	if(axisMode[mainAxisIndex] == AxisMode::Expand && mainAxisFlexibleSpace <= 0.f) {
		if(m_OverflowPolicy == OverflowPolicy::Wrap) {
			// Check if can expand in cross axis and put children there
		} else if(m_OverflowPolicy == OverflowPolicy::ShrinkWrap) {
			// Do not decrease our size below content size
		}
	}

	// #2 Calculate sizes for flexible widgets if possible
	if(totalFlexFactor < 0.f) {
		bJustifyContent = false;

		for(auto& childData: childrenData) {
			if(childData.mainAxisSize < 0.f) {
				childData.mainAxisSize = mainAxisFlexibleSpace * childData.mainAxisSize / totalFlexFactor;
				/// TODO Child size shouldn't be less then Child.GetMinSize()
				childData.mainAxisSize = Math::Clamp(childData.mainAxisSize, 0.f);
			}
		}
	}

	// #3 Calculate the space divisions for content justification
	// This is the margin added to items
	float justifyContentMargin = 0.f;

	if(bJustifyContent && axisMode[mainAxisIndex] == AxisMode::Expand) {
		switch(m_JustifyContent) {
			case JustifyContent::End: {
				justifyContentMargin = mainAxisFlexibleSpace;
				break;
			}
			case JustifyContent::Center: {
				justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
				break;
			}
			case JustifyContent::SpaceBetween: {
				if(childrenData.size() == 1) {
					justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
				} else {
					justifyContentMargin = mainAxisFlexibleSpace / (childrenData.size() - 1);
				}
				break;
			}
			case JustifyContent::SpaceAround: {
				if(childrenData.size() == 1) {
					justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
				} else {
					justifyContentMargin = mainAxisFlexibleSpace / (childrenData.size() + 1);
				}
				break;
			}
		}
	}

	// If our size depends on children, set this to max temporarily
	auto crossAxisSize = axisMode[crossAxisIndex] == AxisMode::Expand
		? innerSize[crossAxisIndex]
		: std::numeric_limits<float>::max();
	auto mainAxisCursor = (float)paddings.TL()[mainAxisIndex];

	if(m_JustifyContent == JustifyContent::Center 
	   || m_JustifyContent == JustifyContent::End 
       || m_JustifyContent == JustifyContent::SpaceAround) {
		mainAxisCursor = justifyContentMargin;
	}
	float maxChildSizeCrossAxis = 0.f;

	// #4 At this point we have shrink shildren already updated
	// and main axis flexibles calculated
	// So we need to update the flexibles and align all widgets later
	for(auto& childData: childrenData) {
		// Cache position for later
		childData.mainAxisPos = mainAxisCursor;

		float2 constraints;
		constraints[mainAxisIndex] = childData.mainAxisSize;
		constraints[crossAxisIndex] = crossAxisSize;
		ParentLayoutEvent layoutEvent;
		layoutEvent.parent = this;
		layoutEvent.constraints = Rect(float2(), constraints);

		// LOGF(Verbose, 
		//     "Flexbox {} dispatched layout event to child {}. Constraints [left: {}, right: {}, top: {}, bottom: {}]", 
		// 	GetDebugID(), 
		// 	childData.child->GetDebugID(),
		// 	layoutEvent.constraints.Left(),
		// 	layoutEvent.constraints.Right(),
		// 	layoutEvent.constraints.Top(),
		// 	layoutEvent.constraints.Bottom());
		float2 widgetSize;

		if(childData.isFlexible) {
			widgetSize = childData.child->OnLayout(layoutEvent);
			childData.crossAxisSize = widgetSize[crossAxisIndex];
		} else {
			// Calculated at #1
			widgetSize = childData.child->GetOuterSize();
		}
		mainAxisCursor += constraints[mainAxisIndex];
		maxChildSizeCrossAxis = Math::Max(maxChildSizeCrossAxis, widgetSize[crossAxisIndex]);

		if(bJustifyContent 
		   && m_JustifyContent == JustifyContent::SpaceBetween 
		   || m_JustifyContent == JustifyContent::SpaceAround) {
			mainAxisCursor += justifyContentMargin;
		}
	}

	// #5 Update our size if it depends on children
	const auto totalMainAxisContentSize = mainAxisCursor;
	if(axisMode[crossAxisIndex] == AxisMode::Shrink) {
		crossAxisSize = maxChildSizeCrossAxis;
		SetSize(crossAxisIndex, crossAxisSize);
	}
	auto mainAxisSize = GetSize()[mainAxisIndex];
	if(axisMode[mainAxisIndex] == AxisMode::Shrink) {
		mainAxisSize = fixedChildrenSizeMainAxis;
	}
	if(axisMode[mainAxisIndex] == AxisMode::Shrink 
	   || (m_OverflowPolicy == OverflowPolicy::ShrinkWrap 
	   && totalMainAxisContentSize > innerMainAxisSize)) {
		SetSize(mainAxisIndex, totalMainAxisContentSize);
	}

	// #6 At this point all sizes are calculated and we can update children positions
	for(auto& childData: childrenData) {
		float2 pos;
		pos[mainAxisIndex] = childData.mainAxisPos;

		if(m_Alignment == Alignment::End) {
			pos[crossAxisIndex] = crossAxisSize - childData.crossAxisSize;
		} else if(m_Alignment == Alignment::Center) {
			pos[crossAxisIndex] = 0.5f * (crossAxisSize - childData.crossAxisSize);
		}		
		const auto margins = childData.child->GetLayoutStyle()->margins;
		childData.child->SetOrigin(pos + margins.TL());
	}	
}



// /*-------------------------------------------------------------------------------------------------*/
// //										GUIDELINE
// /*-------------------------------------------------------------------------------------------------*/
// UI::Guideline::Guideline(bool bIsVertical, OnDraggedFunc inCallback, Widget* inParent, WidgetSlot inSlot)
// 	: m_MainAxis(bIsVertical ? Axis::Y : Axis::X)
// 	, m_State(State::Normal)
// 	, m_Callback(inCallback)
// {
// 	if(inParent) inParent->Parent(this, inSlot);
// }

// bool UI::Guideline::OnEvent(IEvent* inEvent) {

// 	if(auto* event = inEvent->Cast<HoverEvent>()) {
// 		// Change state and style
// 		if(event->bHovered) {
// 			m_State = State::Hovered;
// 		} else {
// 			m_State = State::Normal;
// 		}
// 		return true;
// 	}

// 	if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
// 		float2 size;
// 		size[m_MainAxis] = event->Constraints.Size()[m_MainAxis];
// 		size[InvertAxis(m_MainAxis)] = 10.f;
// 		SetSize(size);
// 		SetOrigin(event->Constraints.TL());
// 		return true;
// 	}

// 	if(auto* event = inEvent->Cast<MouseButtonEvent>()) {

// 		if(event->Button == MouseButton::ButtonLeft) {

// 			if(event->bButtonPressed) {
// 				m_State = State::Pressed;
// 			} else {
// 				m_State = State::Normal;
// 			}
// 			return true;
// 		}
// 		return false;
// 	}

// 	if(auto* event = inEvent->Cast<MouseDragEvent>()) {

// 		if(m_State == State::Pressed) {
// 			const auto delta = event->MouseDelta[InvertAxis(m_MainAxis)];
// 			if(m_Callback) m_Callback(event);
// 		}
// 		return true;
// 	}

// 	if(auto * event = inEvent->Cast<DrawEvent>()) {
// 		auto color = m_State == State::Hovered ? Colors::HoveredDark : m_State == State::Pressed ? Colors::PressedDark : Colors::PrimaryDark;
// 		event->DrawList->PushBox(GetRect(), color, true);
// 		return true;
// 	}
// 	return Super::OnEvent(inEvent);
// }

// void UI::Guideline::DebugSerialize(Debug::PropertyArchive& inArchive) {
// 	Super::DebugSerialize(inArchive);
// 	inArchive.PushProperty("MainAxis", m_MainAxis == Axis::X ? "X" : "Y");
// 	inArchive.PushProperty("State", m_State.String());
// }



// /*-------------------------------------------------------------------------------------------------*/
// //										SPLITBOX
// /*-------------------------------------------------------------------------------------------------*/
// UI::SplitBox::SplitBox(bool bHorizontal, Widget* inParent, WidgetSlot inSlot)
// 	: m_MainAxis(bHorizontal ? Axis::X : Axis::Y)
// 	, m_SplitRatio(0.5f)
// 	, m_Separator()
// {
// 	SetAxisMode(AxisModeExpand);
// 	if(inParent) inParent->Parent(this, inSlot);

// 	m_Separator.reset(new Guideline(bHorizontal, [this](auto* inDragEvent) { OnSeparatorDragged(inDragEvent); }, nullptr));
// 	m_Separator->OnParented(this);
// }

// bool UI::SplitBox::OnEvent(IEvent* inEvent) {

// 	if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
// 		ExpandToParent(event);
// 		UpdateLayout();
// 		return true;
// 	}

// 	if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
// 		UpdateLayout();
// 		return true;
// 	}

// 	if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
// 		drawEvent->DrawList->PushBox(GetRect(), Colors::Green, false);
// 		drawEvent->DrawList->PushTransform(GetOrigin());
// 		DispatchToChildren(inEvent);
// 		drawEvent->DrawList->PopTransform();
// 		return true;
// 	}
// 	return Super::OnEvent(inEvent);
// }

// bool UI::SplitBox::DispatchToChildren(IEvent* inEvent) {
// 	bool bBroadcast = inEvent->IsBroadcast();

// 	for(auto i = 0; i < 3; ++i) {
// 		auto* child = i == 0 ? m_First.get() : i == 1 ? m_Separator.get() : m_Second.get();
// 		const auto bHandled = child->IsVisible() ? child->OnEvent(inEvent) : false;

// 		if(bHandled && !bBroadcast) {
// 			return true;
// 		}
// 	}
// 	return false;
// }

// void UI::SplitBox::DebugSerialize(Debug::PropertyArchive& inArchive) {
// 	Super::DebugSerialize(inArchive);
// 	inArchive.PushProperty("Direction", m_MainAxis == Axis::Y ? "Column" : "Row");
// 	inArchive.PushProperty("SplitRatio", m_SplitRatio);
// }

// UI::VisitResult UI::SplitBox::VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive) {
// 	for(auto i = 0; i < 3; ++i) {
// 		auto* child = i == 0 ? m_First.get() : i == 1 ? m_Separator.get() : m_Second.get();

// 		const auto result = inVisitor(child);
// 		if(!result.bContinue) return VisitResultExit;

// 		if(bRecursive && !result.bSkipChildren) {
// 			const auto result = child->VisitChildren(inVisitor, bRecursive);
// 			if(!result.bContinue) return VisitResultExit;
// 		}
// 	}
// 	return VisitResultContinue;
// }

// void UI::SplitBox::OnSeparatorDragged(MouseDragEvent* inDragEvent) {
// 	const auto mainAxisSize = GetSize(m_MainAxis);
// 	const auto clampedPos = Math::Clamp(inDragEvent->MousePosLocal[m_MainAxis], 10.f, mainAxisSize - 10.f);
// 	const auto posNorm = clampedPos / mainAxisSize;
// 	m_SplitRatio = posNorm;
// 	UpdateLayout();
// }

// void UI::SplitBox::SetSplitRatio(float inRatio) {
// 	const auto ratio = Math::Clamp(inRatio, 0.f, 1.f);
// 	m_SplitRatio = ratio;
// 	UpdateLayout();
// }

// void UI::SplitBox::UpdateLayout() {

// 	LayoutWidget* firstLayoutWidget = nullptr;

// 	if(m_First) {
// 		firstLayoutWidget = m_First->Cast<LayoutWidget>();

// 		if(!firstLayoutWidget) {
// 			firstLayoutWidget = m_First->FindChildOfClass<LayoutWidget>();
// 		}
// 	}

// 	LayoutWidget* secondLayoutWidget = nullptr;

// 	if(m_Second) {
// 		secondLayoutWidget = m_Second->Cast<LayoutWidget>();

// 		if(!secondLayoutWidget) {
// 			secondLayoutWidget = m_Second->FindChildOfClass<LayoutWidget>();
// 		}
// 	}

// 	if(!firstLayoutWidget && !secondLayoutWidget) return;

// 	if(!firstLayoutWidget || !firstLayoutWidget->IsVisible()) {
// 		ParentLayoutEvent onParent(this, Rect(GetSize()));
// 		secondLayoutWidget->OnEvent(&onParent);
// 		m_Separator->SetVisibility(false);

// 	} else if(!secondLayoutWidget || !secondLayoutWidget->IsVisible()) {
// 		ParentLayoutEvent onParent(this, Rect(GetSize()));
// 		firstLayoutWidget->OnEvent(&onParent);
// 		m_Separator->SetVisibility(false);

// 	} else {
// 		m_Separator->SetVisibility(true);

// 		const auto size = GetSize();
// 		const auto mainAxisSize = size[m_MainAxis];
// 		const auto crossAxisSize = size[!m_MainAxis];
// 		const auto separatorThickness = m_Separator->GetOuterSize()[m_MainAxis];
// 		const auto firstChildConstraint = m_SplitRatio * mainAxisSize - separatorThickness;
// 		const auto secondChildConstraint = mainAxisSize - separatorThickness - firstChildConstraint;

// 		float2 childConstraints;
// 		{
// 			childConstraints[m_MainAxis] = firstChildConstraint;
// 			childConstraints[!m_MainAxis] = crossAxisSize;

// 			ParentLayoutEvent onParent(this, Rect(childConstraints));
// 			firstLayoutWidget->OnEvent(&onParent);
// 		}
// 		{
// 			float2 pos;
// 			pos[m_MainAxis] = firstChildConstraint;
// 			pos[!m_MainAxis] = 0.f;

// 			ParentLayoutEvent onParent(this, Rect(pos, size));
// 			m_Separator->OnEvent(&onParent);
// 		}
// 		{
// 			childConstraints[m_MainAxis] = secondChildConstraint;
// 			childConstraints[!m_MainAxis] = crossAxisSize;

// 			float2 secondChildPos;
// 			secondChildPos[m_MainAxis] = firstChildConstraint + separatorThickness;
// 			secondChildPos[!m_MainAxis] = 0.f;

// 			ParentLayoutEvent onParent(this, Rect(secondChildPos, childConstraints));
// 			secondLayoutWidget->OnEvent(&onParent);
// 		}
// 	}
// }



}