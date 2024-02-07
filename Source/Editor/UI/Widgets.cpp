#include "Widgets.h"
#include "UI.h"


/*-------------------------------------------------------------------------------------------------*/
//										CENTERED
/*-------------------------------------------------------------------------------------------------*/
UI::Centered::Centered(Widget* inParent, const std::string& inID) 
	: Super(inID)
{
	Super::SetAxisMode(AxisModeExpand);
	if(inParent) {
		inParent->Parent(this);
	}
}

void UI::Centered::OnParented(Widget* inParent) {
	Super::OnParented(inParent);
	auto layoutParent = Super::GetNearestLayoutParent();		
	Super::SetAxisMode(layoutParent->GetAxisMode());
}

bool UI::Centered::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<ParentLayoutEvent>()) {
		const auto bSizeChanged = Super::ExpandToParent(event);

		if(bSizeChanged) {
			Super::NotifyChildOnSizeChanged();
			Super::CenterChild(event);
		}		
		return true;

	} else if(auto* event = inEvent->As<ChildLayoutEvent>()) {
		Super::HandleChildEvent(event);
		Super::CenterChild(event);
		return true;
	}
	return Super::OnEvent(inEvent);
}



/*-------------------------------------------------------------------------------------------------*/
//										FLEXBOX
/*-------------------------------------------------------------------------------------------------*/
UI::Flexbox* UI::FlexboxBuilder::Parent(Widget* inParent) { m_Parent = inParent; return new Flexbox(*this); }

UI::Flexbox::Flexbox(const FlexboxBuilder& inBuilder)
	: Super(inBuilder.m_ID)
	, m_Direction(inBuilder.m_Direction)
	, m_JustifyContent(inBuilder.m_JustifyContent)
	, m_Alignment(inBuilder.m_Alignment)
	, m_OverflowPolicy(inBuilder.m_OverflowPolicy)
{
	//Assertm(inDesc.bExpandMainAxis || !inDesc.bExpandMainAxis && inDesc.JustifyContent == JustifyContent::Start, "If main axis is shrinked, justification has no effect");

	const auto mainAxis = m_Direction == ContentDirection::Row ? 0 : 1;
	Super::SetAxisMode(mainAxis, inBuilder.m_ExpandMainAxis ? AxisMode::Expand : AxisMode::Shrink);
	Super::SetAxisMode(!mainAxis, inBuilder.m_ExpandCrossAxis ? AxisMode::Expand : AxisMode::Shrink);

	if(inBuilder.m_Parent) inBuilder.m_Parent->Parent(this);
}

bool UI::Flexbox::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<ParentLayoutEvent>()) {
		// Update our size if expanded
		Super::ExpandToParent(event);
		UpdateLayout();
		return true;

	} else if(auto* event = inEvent->As<ChildLayoutEvent>()) {

		if(event->Subtype == ChildLayoutEvent::OnAdded) {
			const auto crossAxis = m_Direction == ContentDirection::Row ? AxisY : AxisX;
			Assertf(Super::GetAxisMode()[crossAxis] == AxisMode::Expand || event->Child->GetAxisMode()[crossAxis] == AxisMode::Shrink,
								"Parent child axis shrink/expand mismatch. A child with an expanded axis has been added to the containter with that axis shrinked."
								"Parent {}, Child {}", Super::GetDebugIDString(), event->Child->GetDebugIDString());
		}
		UpdateLayout();		
		Super::NotifyParentOnSizeChanged();
		return true;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		event->DrawList->PushBox(Super::GetRect(), Colors::Blue, false);
		event->DrawList->PushClipRect(Super::GetRect());
	}
	return Super::OnEvent(inEvent);
}

void UI::Flexbox::Parent(Widget* inChild) {
	Super::Parent(inChild);
}

void UI::Flexbox::DebugSerialize(Debug::PropertyArchive& inArchive) {
	Super::DebugSerialize(inArchive);
	inArchive.PushProperty("Direction", m_Direction);
	inArchive.PushProperty("JustifyContent", m_JustifyContent);
	inArchive.PushProperty("Alignment", m_Alignment);
	inArchive.PushProperty("OverflowPolicy", m_OverflowPolicy);
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
	// We cannot update children until parented
	// Because our size depends on parent constraints
	auto parent = Super::GetParent();
	if(!parent) return;

	// Calculate fixed sizes on the main axis
	// Sum up all fixed size and calculate extra
	// Calculate flexible size on the main axis
	// If have flexibles disable alignment on the main axis
	// Calculate flexible sizes
	// Calculate positions
	// Align on the cross axis
	const bool bDirectionRow = m_Direction == ContentDirection::Row;
	const auto mainAxisIndex = bDirectionRow ? AxisX : AxisY;
	const auto crossAxisIndex = bDirectionRow ? AxisY : AxisX;

	const auto paddings = Super::GetPaddings();
	const auto innerSize = Super::GetSize() - paddings.Size();
	const auto innerMainAxisSize = innerSize[mainAxisIndex];
	const auto innerCrossAxisSize = innerSize[crossAxisIndex];

	const auto axisMode = Super::GetAxisMode();

	// These options require extra space left on the main axis to work so we need to find it
	bool bJustifyContent = m_JustifyContent != JustifyContent::Start;	

	std::vector<LayoutWidget*> visibleChildren;
	Super::GetVisibleChildren(&visibleChildren);

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
				Assertf(axisMode[mainAxisIndex] == AxisMode::Expand, "Flexbox main axis set to AxisMode::Shrink, but an expanded child is found");
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
	float mainAxisCursor = paddings.TL()[mainAxisIndex];

	if(m_JustifyContent == JustifyContent::Center ||
		m_JustifyContent == JustifyContent::End || m_JustifyContent == JustifyContent::SpaceAround) {
		mainAxisCursor = justifyContentMargin;
	}

	for(auto i = 0; i != tempBuffer.size(); ++i) {
		auto& temp = tempBuffer[i];
		auto& child = visibleChildren[i];
		temp.MainAxisPos = mainAxisCursor;

		float2 childPos;
		childPos[mainAxisIndex] = temp.MainAxisPos;
		childPos[crossAxisIndex] = temp.CrossAxisPos;

		float2 childSize;
		childSize[mainAxisIndex] = temp.MainAxisSize;
		childSize[crossAxisIndex] = temp.CrossAxisSize;

		ParentLayoutEvent layoutEvent;
		layoutEvent.Parent = this;
		layoutEvent.Constraints = Rect(childPos, childSize);

		child->OnEvent(&layoutEvent);

		mainAxisCursor += child->GetOuterSize()[mainAxisIndex];

		if(bJustifyContent && m_JustifyContent == JustifyContent::SpaceBetween || m_JustifyContent == JustifyContent::SpaceAround) {
			mainAxisCursor += justifyContentMargin;
		}

		if(m_Alignment == AlignContent::End) {
			temp.CrossAxisPos = maxChildSizeCrossAxis - temp.CrossAxisSize;

		} else if(m_Alignment == AlignContent::Center) {
			temp.CrossAxisPos = 0.5f * (maxChildSizeCrossAxis - temp.CrossAxisSize);
		}
	}
	const auto totalMainAxisContentSize = mainAxisCursor;

	if(axisMode[crossAxisIndex] == AxisMode::Shrink) {
		Super::SetSize(crossAxisIndex, maxChildSizeCrossAxis);
	}

	if(axisMode[mainAxisIndex] == AxisMode::Shrink || m_OverflowPolicy == OverflowPolicy::ShrinkWrap && totalMainAxisContentSize > innerMainAxisSize) {
		Super::SetSize(mainAxisIndex, totalMainAxisContentSize);
	}
}



/*-------------------------------------------------------------------------------------------------*/
//										BUTTON
/*-------------------------------------------------------------------------------------------------*/
UI::Button* UI::ButtonBuilder::Parent(Widget* inParent) {
	m_Parent = inParent;
	return new UI::Button(*this);
}

UI::Button::Button(Widget* inParent, OnPressedFunc inCallback, const std::string& inID)
	: Super(inID)
	, m_Style(Application::Get()->GetTheme()->Find(GetClassName().data()))
	, m_State(ButtonState::Normal)
	, m_Callback(inCallback)
{
	if(inParent) {
		inParent->Parent(this);
	}
}

UI::Button::Button(const ButtonBuilder& inBuilder)
	: Super(inBuilder.m_ID)
	, m_Style(Application::Get()->GetTheme()->Find(GetClassName().data()))
	, m_State(ButtonState::Normal)
	, m_Callback(inBuilder.m_Callback) 
{
	if(inBuilder.m_Parent) {
		inBuilder.m_Parent->Parent(this);
	}
}

bool UI::Button::OnEvent(IEvent* inEvent) {
	// OnLayout is handled by the super
	// Our size and position is handled there
	// Our children usually is Icon or Text so no need to update children

	if(auto* event = inEvent->As<HoverEvent>()) {
		
		// Change state and style
		if(event->bHovered) {
			m_State = ButtonState::Hovered;
		} else {
			m_State = ButtonState::Normal;
		}
		return true;

	} if(auto* event = inEvent->As<MouseButtonEvent>()) {

		if(event->Button == MouseButtonEnum::ButtonLeft) {

			if(event->bButtonPressed) {
				m_State = ButtonState::Pressed;
			} else {
				m_State = ButtonState::Normal;
			}

			if(m_Callback) {
				m_Callback(event->bButtonPressed);
			}
			return true;
		}
		return false;

	} else if(auto* event = inEvent->As<ChildLayoutEvent>()) {
		Super::HandleChildEvent(event);
		Super::NotifyParentOnSizeChanged();
		return true;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		event->DrawList->PushBox(Super::GetRect(), m_Style->Find<BoxStyle>(m_State));
	}
	return Super::OnEvent(inEvent);
}

UI::Padding UI::Button::GetPaddings() const {

	if(m_Style) {
		const auto* layoutStyle = m_Style->Find<LayoutStyle>(m_State);

		if(layoutStyle) {
			return layoutStyle->Paddings;
		}
	}
	return {};
}

UI::Padding UI::Button::GetMargins() const {

	if(m_Style) {
		const auto* layoutStyle = m_Style->Find<LayoutStyle>(m_State);

		if(layoutStyle) {
			return layoutStyle->Margins;
		}
	}
	return {};
}




/*-------------------------------------------------------------------------------------------------*/
//										TEXT
/*-------------------------------------------------------------------------------------------------*/
UI::Text::Text(Widget* inParent, const std::string& inText, const std::string& inID)
	: Super(inID)
	, m_Style(Application::Get()->GetTheme()->Find(GetClassName().data())) 
	, m_Text(inText)
{
	const auto size = m_Style->Find<TextStyle>()->CalculateTextSize(m_Text);
	Super::SetSize(size);

	if(inParent) {
		inParent->Parent(this);
	}
}

void UI::Text::SetText(const std::string& inText) {
	m_Text = inText;
	float2 size;

	if(inText.empty()) {
		/// TODO handle the case when the text is empty
		/// Maybe do not update parent
		/// For now we set it to some random values
		size = float2(15, 15);
	} else {
		size = m_Style->Find<TextStyle>()->CalculateTextSize(m_Text);
		Super::SetSize(size);
	}
	Super::NotifyParentOnSizeChanged();
}

bool UI::Text::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<HoverEvent>()) {
		// Ignore, we cannot be hovered
		return false;

	} else if(auto* event = inEvent->As<ParentLayoutEvent>()) {
		// Ignore size
		Super::SetOrigin(event->Constraints.TL());
		return false;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		event->DrawList->PushText(Super::GetOrigin(), m_Style->Find<TextStyle>(), m_Text);
	}
	return Super::OnEvent(inEvent);
}



/*-------------------------------------------------------------------------------------------------*/
//										GUIDELINE
/*-------------------------------------------------------------------------------------------------*/
UI::Guideline::Guideline(Widget* inParent, bool bIsVertical /*= true*/, OnDraggedFunc inCallback /*= {}*/, const std::string& inID /*= {}*/) 
	: Super(std::string(inID))
	, m_MainAxis(bIsVertical ? AxisY : AxisX)
	, m_State(ButtonState::Normal)
	, m_Callback(inCallback)
{
	if(inParent) inParent->Parent(this);
}

bool UI::Guideline::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<HoverEvent>()) {

		// Change state and style
		if(event->bHovered) {
			m_State = ButtonState::Hovered;
		} else {
			m_State = ButtonState::Normal;
		}
		return true;

	} else if(auto* event = inEvent->As<ParentLayoutEvent>()) {
		float2 size;
		size[m_MainAxis] = event->Constraints.Size()[m_MainAxis];
		size[!m_MainAxis] = 5.f;
		Super::SetSize(size);
		Super::SetOrigin(event->Constraints.TL());
		return true;

	} else if(auto* event = inEvent->As<MouseButtonEvent>()) {

		if(event->Button == MouseButtonEnum::ButtonLeft) {

			if(event->bButtonPressed) {
				m_State = ButtonState::Pressed;
			} else {
				m_State = ButtonState::Normal;
			}
			return true;
		}
		return false;

	} else if(auto* event = inEvent->As<MouseDragEvent>()) {
		
		if(m_State == ButtonState::Pressed) {
			const auto delta = event->Delta[!m_MainAxis];
			if(m_Callback) m_Callback(event);
		}
		return true;

	} else if(auto * event = inEvent->As<DrawEvent>()) {
		auto color = m_State == ButtonState::Hovered ? Colors::HoveredDark : m_State == ButtonState::Pressed ? Colors::PressedDark : Colors::PrimaryDark;
		event->DrawList->PushBox(Super::GetRect(), color, true);
		return true;
	}
	return Super::OnEvent(inEvent);
}




/*-------------------------------------------------------------------------------------------------*/
//										SPLITBOX
/*-------------------------------------------------------------------------------------------------*/
UI::SplitBox::SplitBox(Widget* inParent, bool bHorizontal /*= true*/, const std::string& inID /*= {}*/) 
	: Super(inID)
	, m_MainAxis(bHorizontal ? AxisX : AxisY)
	, m_SplitRatio(0.5f)
{
	new Guideline(
		this,
		bHorizontal,
		[this](MouseDragEvent* inDragEvent) { OnSeparatorDragged(inDragEvent); },
		std::string(Super::GetID()).append("::Separator"));

	Super::SetAxisMode(AxisModeExpand);
	if(inParent) inParent->Parent(this);
}

void UI::SplitBox::Parent(Widget* inChild) {
	Assert(inChild && !m_First || !m_Second);

	if(!m_Separator) {
		m_Separator.reset(inChild->As<Guideline>());
		m_Separator->OnParented(this);

	} else  if(!m_First) {
		m_First.reset(inChild);
		m_First->OnParented(this);

	} else if(!m_Second) {
		m_Second.reset(inChild);
		m_Second->OnParented(this);

	} else {
		Assertm(false, "A Splitbox can contain only two children");
	}
}

void UI::SplitBox::Unparent(Widget* inChild) {
	Assert(inChild && inChild == m_First.get() || inChild == m_Second.get());

	if(inChild == m_First.get()) {
		m_First.release();
	} else {
		m_Second.release();
	}
}

bool UI::SplitBox::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<ParentLayoutEvent>()) {
		Super::ExpandToParent(event);
		UpdateLayout();
		return true;

	} else if(auto* event = inEvent->As<ChildLayoutEvent>()) {
		UpdateLayout();
		return true;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		event->DrawList->PushBox(Super::GetRect(), Colors::Green, false);
		return true;
	}
	return Super::OnEvent(inEvent);
}

bool UI::SplitBox::DispatchToChildren(IEvent* inEvent) {
	bool bHandled = false;
	bool bBroadcast = inEvent->IsBroadcast();

	if(m_First) {
		bHandled = m_First->OnEvent(inEvent);
	}

	if(bHandled && !bBroadcast) return true;
		
	if(m_Second) {
		bHandled = m_Second->OnEvent(inEvent);
	}

	if(bHandled && !bBroadcast) return true;

	if(m_Separator) {
		bHandled = m_Separator->OnEvent(inEvent);
	}

	if(bHandled && !bBroadcast) return true;
	return false;
}

void UI::SplitBox::DebugSerialize(Debug::PropertyArchive& inArchive) {
	Super::DebugSerialize(inArchive);
	inArchive.PushProperty("Direction", m_MainAxis ? "Column" : "Row");
	inArchive.PushProperty("SplitRatio", m_SplitRatio);
}

UI::VisitResult UI::SplitBox::VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive) {
	for(auto i = 0; i < 3; ++i) {
		auto* child = i == 0 ? m_First.get() : i == 1 ? m_Separator.get() : m_Second.get();

		const auto result = inVisitor(child);
		if(!result.bContinue) return VisitResultExit;

		if(bRecursive && !result.bSkipChildren) {
			const auto result = child->VisitChildren(inVisitor, bRecursive);
			if(!result.bContinue) return VisitResultExit;
		}
	}
	return VisitResultContinue;
}

void UI::SplitBox::OnSeparatorDragged(MouseDragEvent* inDragEvent) {
	const auto mainAxisSize = Super::GetSize(m_MainAxis);
	const auto clampedPos = Math::Clamp(inDragEvent->PosLocal[m_MainAxis], 10.f, mainAxisSize - 10.f);
	const auto posNorm = clampedPos / mainAxisSize;
	m_SplitRatio = posNorm;
	UpdateLayout();
}

void UI::SplitBox::SetSplitRatio(float inRatio) {
	const auto ratio = Math::Clamp(inRatio, 0.f, 1.f);
	m_SplitRatio = ratio;
	UpdateLayout();
}

void UI::SplitBox::UpdateLayout() {

	LayoutWidget* firstLayoutWidget = nullptr;

	if(m_First) {
		firstLayoutWidget = m_First->As<LayoutWidget>();

		if(!firstLayoutWidget) {
			firstLayoutWidget = m_First->GetChild<LayoutWidget>();
		}
	}

	LayoutWidget* secondLayoutWidget = nullptr;

	if(m_Second) {
		secondLayoutWidget = m_Second->As<LayoutWidget>();

		if(!secondLayoutWidget) {
			secondLayoutWidget = m_Second->GetChild<LayoutWidget>();
		}
	}

	if(!firstLayoutWidget && !secondLayoutWidget) return;

	if(!firstLayoutWidget || !firstLayoutWidget->IsVisible()) {
		ParentLayoutEvent onParent(this, Rect(Super::GetSize()));
		secondLayoutWidget->OnEvent(&onParent);
		m_Separator->SetVisibility(false);

	} else if(!secondLayoutWidget || !secondLayoutWidget->IsVisible()) {
		ParentLayoutEvent onParent(this, Rect(Super::GetSize()));
		firstLayoutWidget->OnEvent(&onParent);
		m_Separator->SetVisibility(false);

	} else {		
		m_Separator->SetVisibility(true);

		const auto size = Super::GetSize();
		const auto mainAxisSize = size[m_MainAxis];
		const auto crossAxisSize = size[!m_MainAxis];
		const auto separatorThickness = m_Separator->GetOuterSize()[m_MainAxis];
		const auto firstChildConstraint = m_SplitRatio * mainAxisSize - separatorThickness;
		const auto secondChildConstraint = mainAxisSize - separatorThickness - firstChildConstraint;

		float2 childConstraints;
		{
			childConstraints[m_MainAxis] = firstChildConstraint;
			childConstraints[!m_MainAxis] = crossAxisSize;

			ParentLayoutEvent onParent(this, Rect(childConstraints));
			firstLayoutWidget->OnEvent(&onParent);
		}
		{
			float2 pos;
			pos[m_MainAxis] = firstChildConstraint;
			pos[!m_MainAxis] = 0.f;

			ParentLayoutEvent onParent(this, Rect(pos, size));
			m_Separator->OnEvent(&onParent);
		}
		{
			childConstraints[m_MainAxis] = secondChildConstraint;
			childConstraints[!m_MainAxis] = crossAxisSize;

			float2 secondChildPos;
			secondChildPos[m_MainAxis] = firstChildConstraint + separatorThickness;
			secondChildPos[!m_MainAxis] = 0.f;

			ParentLayoutEvent onParent(this, Rect(secondChildPos, childConstraints));
			secondLayoutWidget->OnEvent(&onParent);
		}
	}
}




/*-------------------------------------------------------------------------------------------------*/
//										TOOLTIP
/*-------------------------------------------------------------------------------------------------*/
UI::Tooltip::Tooltip(float2 inSize /*= {}*/)
	: Super(std::string(GetClassName()))
	, m_Style(Application::Get()->GetTheme()->Find(GetClassName()))
{}

bool UI::Tooltip::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<ChildLayoutEvent>()) {
		Super::HandleChildEvent(event);
		Super::NotifyParentOnSizeChanged();
		return true;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		event->DrawList->PushBox(Super::GetRect(), m_Style->Find<BoxStyle>());
	}
	return Super::OnEvent(inEvent);
}

UI::Padding UI::Tooltip::GetPaddings() const {

	if(m_Style) {
		const auto* layoutStyle = m_Style->Find<LayoutStyle>();

		if(layoutStyle) {
			return layoutStyle->Paddings;
		}
	}
	return {5};
}

UI::Padding UI::Tooltip::GetMargins() const {

	if(m_Style) {
		const auto* layoutStyle = m_Style->Find<LayoutStyle>();

		if(layoutStyle) {
			return layoutStyle->Margins;
		}
	}
	return {5};
}




UI::TooltipSpawner::TooltipSpawner(Widget* inParent, const SpawnerFunction& inSpawner) 
	: Super()
	, m_Spawner(inSpawner)
{
	if(inParent) inParent->Parent(this);
}

UI::LayoutWidget* UI::TooltipSpawner::Spawn() {
	if(m_Spawner) return m_Spawner();
	return nullptr;
}
