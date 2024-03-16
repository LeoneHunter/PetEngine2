#include "widgets.h"
#include "runtime/system/renderer/ui_renderer.h"
#include "ui.h"


/*-------------------------------------------------------------------------------------------------*/
//										TEXT
/*-------------------------------------------------------------------------------------------------*/
UI::Text::Text(const std::string& inText, StringID inStyle)
	: m_Style(Application::Get()->GetTheme()->Find(inStyle))
	, m_Text(inText) {
	const auto size = m_Style->Find<TextStyle>()->CalculateTextSize(m_Text);
	SetSize(size);
}

void UI::Text::SetText(const std::string& inText) {
	m_Text = inText;
	float2 size;

	if(inText.empty()) {
		// TODO handle the case when the text is empty
		// Maybe do not update parent
		// For now we set it to some random values
		size = float2(15, 15);
	} else {
		size = m_Style->Find<TextStyle>()->CalculateTextSize(m_Text);
		SetSize(size);
	}
	DispatchLayoutToParent();
}

bool UI::Text::OnEvent(IEvent* inEvent) {
	if(auto* event = inEvent->Cast<HoverEvent>()) {
		// Ignore, we cannot be hovered
		return false;
	}

	if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
		// Ignore size
		SetOrigin(event->constraints.TL());
		return false;
	}

	if(auto* event = inEvent->Cast<DrawEvent>()) {
		event->canvas->DrawText(GetOrigin(), m_Style->Find<TextStyle>(), m_Text);
		return true;
	}
	return Super::OnEvent(inEvent);
}



/*-------------------------------------------------------------------------------------------------*/
//										WINDOW
/*-------------------------------------------------------------------------------------------------*/
UI::Window::Window(const WindowConfig& inConfig)
	: Super(inConfig.id)
	, m_Style(Application::Get()->GetTheme()->Find(inConfig.style)) {

	auto* titleBar = MouseRegion::Build()
		.HandleButtonAlways(true)
		.OnMouseDrag([this](const MouseDragEvent& e) {
			FindChildOfClass<Container>()->Translate(e.mouseDelta);
		})
		.Child(Container::New(
			ContainerFlags::HorizontalExpand | ContainerFlags::ClipVisibility,
			"TitleBar",
			Flexbox::Build()
				.DirectionRow()
				.ID("TitleBarFlexbox")
				.Style("TitleBar")
				.JustifyContent(JustifyContent::SpaceBetween)
				.AlignCenter()
				.Children({
					Text::New("Window title"),
					Button::New("CloseButton", {}, Text::New("X"))
				})
				.New()))	
		.New();	

	auto* content = Flexbox::Build()
		.ID("ContentArea")
		.DirectionColumn()
		.JustifyContent(JustifyContent::Start)
		.AlignStart()
		.Expand()
		.New();
	
	auto* root = Container::NewFixed(
		ContainerFlags::ClipVisibility,
		inConfig.size,
		inConfig.style,
		TooltipPortal::New(
			[](const TooltipBuildContext& ctx) { 
				return Tooltip::NewText(std::format("Item ID: {}", ctx.sourceWidget->FindChildOfClass<MouseRegion>()->GetDebugID())); 
			},
			MouseRegion::Build()
				.HandleHoverAlways(true)
				.HandleButtonAlways(true)
				.OnMouseButton([this](const MouseButtonEvent& e) {
					if(e.bPressed) {
						Application::Get()->BringToFront(this);
					}
				})
				.OnMouseEnter([this]() {
					FindChildOfClass<Container>()->SetBoxStyleName("Hovered");
				})
				.OnMouseLeave([this]() {
					FindChildOfClass<Container>()->SetBoxStyleName("");
				})
				.Child(Flexbox::Build()
					.DirectionColumn()
					.JustifyContent(JustifyContent::Start)
					.Alignment(AlignContent::Center)
					.Expand()
					.Children({
						titleBar,
						content
					})
					.New()
				)
				.New()
		)
	);
	root->SetOrigin(inConfig.pos);
	SetChild(root);

	for(auto* child: inConfig.children) {
		content->AddChild(child);
	}
}

// Point UI::Window::TransformLocalToGlobal(Point inPosition) const {
// 	return inPosition + GetOrigin();
// }

bool UI::Window::OnEvent(IEvent* inEvent) {
	return Super::OnEvent(inEvent);
}



// /*-------------------------------------------------------------------------------------------------*/
// //										CENTERED
// /*-------------------------------------------------------------------------------------------------*/
// UI::Centered::Centered(Widget* inParent, WidgetSlot inSlot) {
// 	SetAxisMode(AxisModeExpand);
// 	if(inParent) inParent->Parent(this, inSlot);
// }

// void UI::Centered::OnParented(Widget* inParent) {
// 	Super::OnParented(inParent);
// 	auto layoutParent = GetParent<LayoutWidget>();
// 	SetAxisMode(layoutParent->GetAxisMode());
// }

// bool UI::Centered::OnEvent(IEvent* inEvent) {

// 	if(auto* layoutEvent = inEvent->Cast<ParentLayoutEvent>()) {
// 		const auto bSizeChanged = ExpandToParent(layoutEvent);

// 		if(bSizeChanged) {
// 			NotifyChildOnSizeChanged();
// 			CenterChild(layoutEvent);
// 		}
// 		return true;
// 	}

// 	if(auto* childEvent = inEvent->Cast<ChildLayoutEvent>()) {
// 		HandleChildEvent(childEvent);
// 		CenterChild(childEvent);
// 		return true;
// 	}
// 	return Super::OnEvent(inEvent);
// }



/*-------------------------------------------------------------------------------------------------*/
//										FLEXBOX
/*-------------------------------------------------------------------------------------------------*/
UI::Flexbox::Flexbox(const FlexboxConfig& inConfig)
	: Super(Application::Get()->GetTheme()->Find(inConfig.style)->FindOrDefault<LayoutStyle>(),
			axisModeShrink,
			inConfig.id)
	, m_Direction(inConfig.direction)
	, m_JustifyContent(inConfig.justifyContent)
	, m_Alignment(inConfig.alignment)
	, m_OverflowPolicy(inConfig.overflowPolicy)
{
	const auto mainAxis = m_Direction == ContentDirection::Row ? Axis::X : Axis::Y;
	SetAxisMode(mainAxis, inConfig.expandMainAxis ? AxisMode::Expand : AxisMode::Shrink);
	SetAxisMode(InvertAxis(mainAxis), inConfig.expandCrossAxis ? AxisMode::Expand : AxisMode::Shrink);
}

bool UI::Flexbox::OnEvent(IEvent* inEvent) {
	if(auto* e = inEvent->Cast<ParentLayoutEvent>()) {
		// Update our size if expanded
		HandleParentLayoutEvent(e);
		UpdateLayout();
		return true;
	}
	if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
		UpdateLayout();
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

struct TempData {
	float CrossAxisPos = 0;
	float MainAxisPos = 0;
	float CrossAxisSize = 0;
	// Negative for flexible, positive for fixed
	float MainAxisSize = 0;
};

/*
* TODO
*	Handle overflow
*	Optimize for single child added case
*	Optimize for per axis changes
*/
void UI::Flexbox::UpdateLayout() {
	// Calculate fixed sizes on the main axis
	// Sum up all fixed size and calculate extra
	// Calculate flexible size on the main axis
	// If have flexibles disable alignment on the main axis
	// Calculate flexible sizes
	// Calculate positions
	// Align on the cross axis
	const auto axisMode = GetAxisMode();
	const bool bDirectionRow = m_Direction == ContentDirection::Row;
	const auto mainAxisIndex = bDirectionRow ? Axis::X : Axis::Y;
	const auto crossAxisIndex = bDirectionRow ? Axis::Y : Axis::X;
	const auto paddings = GetLayoutStyle() ? GetLayoutStyle()->paddings : Paddings{};
	const auto innerSize = GetSize() - paddings.Size();
	const auto innerMainAxisSize = innerSize[mainAxisIndex];
	const auto innerCrossAxisSize = innerSize[crossAxisIndex];
	// These options require extra space left on the main axis to work so we need to find it
	bool bJustifyContent = m_JustifyContent != JustifyContent::Start;

	std::vector<LayoutWidget*> visibleChildren;
	GetVisibleChildren(&visibleChildren);

	std::vector<TempData> tempBuffer;
	tempBuffer.resize(visibleChildren.size());

	// Size of the biggest widget on the cross axis
	// Our size and sizes of expaned widgets should be this
	float maxChildSizeCrossAxis = 0.f;
	float totalFlexFactor = 0;
	// Widgets which size doesn't depend on our size
	float staticChildrenSizeMainAxis = 0;

	// Find available space after fixed size widgets
	for(auto i = 0; i < visibleChildren.size(); ++i) {
		auto* child = visibleChildren[i];
		auto& temp = tempBuffer[i];

		// Main axis
		/*if(auto* flexible = child->As<Flexible>()) {
			Assertf(axisMode[mainAxisIndex] == AxisMode::Expand, "Flexbox main axis set to AxisMode::Shrink, but an expanded child is found");
			temp.MainAxisSize = -flexible->GetFlexFactor();
			totalFlexFactor += temp.MainAxisSize;

		} else*/ {
			const auto childAxisMode = child->GetAxisMode()[mainAxisIndex];

			if(childAxisMode == AxisMode::Expand) {
				Assertf(axisMode[mainAxisIndex] == AxisMode::Expand,
						"Flexbox main axis set to AxisMode::Shrink, but an expanded child is found");
				temp.MainAxisSize = -1.f;
				totalFlexFactor += temp.MainAxisSize;

			} else {
				temp.MainAxisSize = child->GetOuterSize()[mainAxisIndex];
				staticChildrenSizeMainAxis += temp.MainAxisSize;
			}
		}
		// Cross axis
		const auto childAxisMode = child->GetAxisMode()[crossAxisIndex];

		if(childAxisMode == AxisMode::Expand) {
			temp.CrossAxisSize = innerCrossAxisSize;
		} else {
			temp.CrossAxisSize = child->GetOuterSize()[crossAxisIndex];
		}
		maxChildSizeCrossAxis = Math::Max(temp.CrossAxisSize, maxChildSizeCrossAxis);
	}
	float mainAxisFlexibleSpace = Math::Clamp(innerMainAxisSize - staticChildrenSizeMainAxis, 0.f);

	// Check for overflow
	/// TODO use min size from theme here
	if(axisMode[mainAxisIndex] == AxisMode::Expand && mainAxisFlexibleSpace <= 0.f) {
		if(m_OverflowPolicy == OverflowPolicy::Wrap) {
			// Check if can expand in cross axis and put children there
		} else if(m_OverflowPolicy == OverflowPolicy::ShrinkWrap) {
			// Do not decrease our size below content size
		}
	}

	// Calculate sizes for flexible widgets if possible
	if(totalFlexFactor < 0.f) {
		bJustifyContent = false;

		for(auto i = 0; i < visibleChildren.size(); ++i) {
			auto& temp = tempBuffer[i];

			if(temp.MainAxisSize < 0.f) {
				temp.MainAxisSize = mainAxisFlexibleSpace * temp.MainAxisSize / totalFlexFactor;
				/// TODO Child size shouldn't be less then Child.GetMinSize()
				temp.MainAxisSize = Math::Clamp(temp.MainAxisSize, 0.f);
			}
		}
	}

	// Calculate the space divisions for content justification
	// This is the margin added to items
	float justifyContentMargin = 0.f;

	if(bJustifyContent && axisMode[mainAxisIndex] == AxisMode::Expand) {
		switch(m_JustifyContent) {
			case JustifyContent::End:
			{
				justifyContentMargin = mainAxisFlexibleSpace;
				break;
			}
			case JustifyContent::Center:
			{
				justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
				break;
			}
			case JustifyContent::SpaceBetween:
			{
				if(tempBuffer.size() == 1) {
					justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
				} else {
					justifyContentMargin = mainAxisFlexibleSpace / (tempBuffer.size() - 1);
				}
				break;
			}
			case JustifyContent::SpaceAround:
			{
				if(tempBuffer.size() == 1) {
					justifyContentMargin = mainAxisFlexibleSpace * 0.5f;
				} else {
					justifyContentMargin = mainAxisFlexibleSpace / (tempBuffer.size() + 1);
				}
				break;
			}
		}
	}
	// Lay out children
	auto mainAxisCursor = (float)paddings.TL()[mainAxisIndex];

	if(m_JustifyContent == JustifyContent::Center ||
		m_JustifyContent == JustifyContent::End || m_JustifyContent == JustifyContent::SpaceAround) {
		mainAxisCursor = justifyContentMargin;
	}

	for(auto i = 0; i != tempBuffer.size(); ++i) {
		auto& temp = tempBuffer[i];
		auto& child = visibleChildren[i];
		temp.MainAxisPos = mainAxisCursor;

		if(m_Alignment == AlignContent::End) {
			temp.CrossAxisPos = maxChildSizeCrossAxis - temp.CrossAxisSize;
		} else if(m_Alignment == AlignContent::Center) {
			temp.CrossAxisPos = 0.5f * (maxChildSizeCrossAxis - temp.CrossAxisSize);
		}

		float2 childPos;
		childPos[mainAxisIndex] = temp.MainAxisPos;
		childPos[crossAxisIndex] = temp.CrossAxisPos;

		float2 childSize;
		childSize[mainAxisIndex] = temp.MainAxisSize;
		childSize[crossAxisIndex] = temp.CrossAxisSize;

		ParentLayoutEvent layoutEvent;
		layoutEvent.parent = this;
		layoutEvent.constraints = Rect(childPos, childSize);

		LOGF(Verbose, "Flexbox {} dispatched layout event to child {}. Constraints [left: {}, right: {}, top: {}, bottom: {}]", 
			GetDebugID(), 
			child->GetDebugID(),
			layoutEvent.constraints.Left(),
			layoutEvent.constraints.Right(),
			layoutEvent.constraints.Top(),
			layoutEvent.constraints.Bottom());

		child->OnEvent(&layoutEvent);

		mainAxisCursor += child->GetOuterSize()[mainAxisIndex];

		if(bJustifyContent && m_JustifyContent == JustifyContent::SpaceBetween || m_JustifyContent == JustifyContent::SpaceAround) {
			mainAxisCursor += justifyContentMargin;
		}
	}
	const auto totalMainAxisContentSize = mainAxisCursor;

	if(axisMode[crossAxisIndex] == AxisMode::Shrink) {
		SetSize(crossAxisIndex, maxChildSizeCrossAxis);
	}
	if(axisMode[mainAxisIndex] == AxisMode::Shrink || m_OverflowPolicy == OverflowPolicy::ShrinkWrap && totalMainAxisContentSize > innerMainAxisSize) {
		SetSize(mainAxisIndex, totalMainAxisContentSize);
	}
	LOGF(Verbose, "Flexbox {} layout has been updated. Flexbox size: {}", GetDebugID(), GetSize());
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





// /*-------------------------------------------------------------------------------------------------*/
// //										POPUP
// /*-------------------------------------------------------------------------------------------------*/
// UI::PopupWindow::PopupWindow(const PopupBuilder& inBuilder)
// 	: Window(
// 		WindowBuilder()
// 		.Position(inBuilder.m_Pos)
// 		.Flags(WindowFlags::Popup)
// 		.Size(inBuilder.m_Size)
// 		.ID(inBuilder.m_ID)
// 		.StyleClass(inBuilder.m_StyleClass))
// 	, m_Spawner(nullptr)
// 	, m_NextPopup()
// 	, m_NextPopupSpawner(nullptr)
// {}

// UI::PopupWindow::~PopupWindow() {
// 	m_Spawner->OnPopupDestroyed();
// }

// bool UI::PopupWindow::OnEvent(IEvent* inEvent) {

// 	if(auto* mouseButtonEvent = inEvent->Cast<MouseButtonEvent>()) {

// 		// Close if clicked outside the window
// 		if(!GetRect().Contains(mouseButtonEvent->MousePosGlobal)) {
// 			auto msg = PopupEvent::ClosePopup(this);
// 			DispatchToParent(&msg);
// 			return true;
// 		}
// 		return false;
// 	}

// 	if(auto* hoverEvent = inEvent->Cast<HoverEvent>()) {

// 		if(m_NextPopup && m_NextPopup->OnEvent(inEvent)) {
// 			return false;
// 		}
// 		auto bHandledByChildren = DispatchToChildren(inEvent);

// 		if(!bHandledByChildren && !GetParent<PopupWindow>()) {
// 			return true;
// 		}
// 		return bHandledByChildren;
// 	}

// 	if(auto* hitTest = inEvent->Cast<HitTestEvent>()) {

// 		if(m_NextPopup && m_NextPopup->OnEvent(inEvent)) {
// 			return true;
// 		}
// 		auto hitPosLocalSpace = hitTest->GetLastHitPos();
// 		const auto bThisHovered = GetRect().Contains(hitPosLocalSpace);

// 		if(!GetParent<PopupWindow>() || bThisHovered) {
// 			hitTest->PushItem(this, hitPosLocalSpace - GetOrigin());
// 		}

// 		if(bThisHovered) {
// 			DispatchToChildren(inEvent);
// 		}
// 		// Because we are a Popup, block this event for propagating
// 		// even if we aren't hovered and have no parent popup
// 		return !GetParent<PopupWindow>() || bThisHovered;
// 	}

// 	// BUGFIX Because we are a parent for a submenu popup,
// 	//  we receive this event when a popup is created,
// 	//	but it's not part of our layout, so ignore
// 	if(auto* childEvent = inEvent->Cast<ChildLayoutEvent>()) {

// 		if((childEvent->Subtype == ChildLayoutEvent::OnAdded
// 		   || childEvent->Subtype == ChildLayoutEvent::OnRemoved)
// 		   && childEvent->Child == m_NextPopup.get()) {
// 			return true;
// 		}
// 		return Super::OnEvent(inEvent);
// 	}

// 	if(auto* popupEvent = inEvent->Cast<PopupEvent>()) {

// 		if(popupEvent->Type == PopupEvent::Type::Open) {
// 			const auto ctx = Application::GetFrameState();
// 			m_NextPopup = popupEvent->Spawner->OnSpawn(ctx.MousePosGlobal, ctx.WindowSize);
// 			m_NextPopup->OnParented(this);
// 			m_NextPopupSpawner = popupEvent->Spawner;
// 			return true;
// 		}

// 		if(popupEvent->Type == PopupEvent::Type::Close) {
// 			Assert(popupEvent->Popup == m_NextPopup.get());
// 			m_NextPopup.reset();
// 			m_NextPopupSpawner = nullptr;
// 			return true;
// 		}

// 		if(popupEvent->Type == PopupEvent::Type::CloseAll) {
// 			// Close all popup stack
// 			auto msg = PopupEvent::CloseAll();
// 			DispatchToParent(&msg);
// 			return true;
// 		}
// 	}

// 	if(auto* layoutEvent = inEvent->Cast<ParentLayoutEvent>()) {
// 		const auto viewportSize = layoutEvent->Constraints.Size();
// 		const auto popupRect = GetRect();
// 			  auto newOrigin = GetOrigin();

// 		for(auto axis: Axes2D) {
// 			if(popupRect.max[axis] > viewportSize[axis]) {
// 				newOrigin[axis] = viewportSize[axis] - popupRect.Size()[axis];
// 			}
// 		}
// 		SetOrigin(newOrigin);
// 		return true;
// 	}

// 	if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
// 		Super::OnEvent(inEvent);
// 		if(m_NextPopup) m_NextPopup->OnEvent(inEvent);
// 		return true;
// 	}

// 	return Super::OnEvent(inEvent);
// }

// void UI::PopupWindow::OnItemHovered() {
// 	if(m_NextPopupSpawner) {
// 		m_NextPopupSpawner->ClosePopup();
// 	}
// 	m_NextPopup.reset();
// 	m_NextPopupSpawner = nullptr;
// }

// void UI::PopupWindow::OnItemPressed() {
// 	// When pressed, closes the stack
// 	auto msg = PopupEvent::CloseAll();
// 	DispatchToParent(&msg);
// }

// UI::WeakPtr<UI::PopupWindow> UI::PopupWindow::OpenPopup(PopupSpawner* inSpawner) {
// 	const auto ctx = Application::GetFrameState();
// 	m_NextPopup = inSpawner->OnSpawn(ctx.MousePosGlobal, ctx.WindowSize);
// 	m_NextPopup->OnParented(this);
// 	m_NextPopupSpawner = inSpawner;
// 	return m_NextPopup->GetWeak();
// }



// /*-------------------------------------------------------------------------------------------------*/
// //										CONTEXT MENU
// /*-------------------------------------------------------------------------------------------------*/
// UI::ContextMenu::ContextMenu(const ContextMenuBuilder& inBuilder)
// 	: PopupWindow(PopupBuilder()
// 		.StyleClass(inBuilder.m_StyleClass)
// 		.Position(inBuilder.m_Pos)
// 		.ID(inBuilder.m_ID))
// 	, m_Container(nullptr)
// {
// 	constexpr auto menuWidthPx = 300;
// 	SetSize(menuWidthPx, 0);
// 	SetAxisMode({AxisMode::Expand, AxisMode::Shrink});

// 	m_Container = FlexboxBuilder()
// 		.DirectionColumn()
// 		.ExpandCrossAxis(true)
// 		.ExpandMainAxis(false)
// 		.JustifyContent(JustifyContent::Center)
// 		.Create(nullptr);

// 	Super::Parent(m_Container);

// 	for(auto& child : inBuilder.m_Children) {
// 		Parent(child.release());
// 	}
// }

// void UI::ContextMenu::Parent(Widget* inWidget, WidgetSlot inSlot) {
// 	Assertm(inWidget->IsA<ContextMenuItem>() || inWidget->FindChildOfClass<ContextMenuItem>()
// 			|| inWidget->IsA<SubMenuItem>() || inWidget->FindChildOfClass<SubMenuItem>(),
// 			"A children of a ContextMenu should be a ContextMenuItem widgets");
// 	m_Container->Parent(inWidget);
// }

// UI::ContextMenuItem::ContextMenuItem(const std::string& inText, ContextMenu* inParent) {
// 	auto* child = ButtonBuilder().Text(inText).Create(this);
// 	child->SetAxisMode({AxisMode::Expand, AxisMode::Shrink});
// 	if(inParent) inParent->Parent(this);
// }

// bool UI::ContextMenuItem::OnEvent(IEvent* inEvent) {
// 	return Super::OnEvent(inEvent);
// }

// UI::SubMenuItem::SubMenuItem(const std::string& inText, const PopupSpawnFunc& inSpawner, ContextMenu* inParent)
// 	: Super(inSpawner, PopupSpawner::SpawnEventType::LeftMouseRelease)
// {
// 	auto* child = ButtonBuilder().Text(inText).Create(this);
// 	child->SetAxisMode({AxisMode::Expand, AxisMode::Shrink});
// 	if(inParent) inParent->Parent(this);
// }

// bool UI::SubMenuItem::OnEvent(IEvent* inEvent) {
// 	constexpr unsigned delayMs = 1500;
// 	static TimerHandle hTimer{};

// 	if(auto* hoverEvent = inEvent->Cast<HoverEvent>()) {
// 		const auto bHandledByChild = DispatchToChildren(inEvent);

// 		if(hoverEvent->bHovered && bHandledByChild) {
// 			auto popup = GetParent<PopupWindow>()->OpenPopup(this);

// 			if(popup) {
// 				constexpr auto popupHorizontalfloat2Px = 5;
// 				const auto rootWindowSize = Application::GetFrameState().WindowSize;
// 				const auto childRect = FindChildOfClass<LayoutWidget>()->GetRectGlobal();
// 				const auto popupSize = popup->GetSize();
// 				Point popupPos;
// 				popupPos.y = childRect.Top();
// 				popupPos.x = childRect.Right() - popupHorizontalfloat2Px;

// 				if(popupPos.x + popupSize.x >= rootWindowSize.x) {
// 					popupPos.x = childRect.Left() - popupSize.x + popupHorizontalfloat2Px;
// 					popupPos.x = Math::Clamp(popupPos.x, 0.f);
// 				}

// 				if(popupPos.y + popupSize.y > rootWindowSize.y) {
// 					popupPos.y = Math::Clamp(rootWindowSize.y - popupSize.y, 0.f);
// 				}
// 				popup->SetOrigin(popupPos);
// 			}
// 		}
// 		return bHandledByChild;
// 	}
// 	return Super::OnEvent(inEvent);
// }
