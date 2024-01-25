#include "Widgets.h"
#include "UI.h"


/*-------------------------------------------------------------------------------------------------*/
//										CENTERED
/*-------------------------------------------------------------------------------------------------*/
UI::Centered::Centered(Widget* inParent, const std::string& inID) 
	: SingleChildContainer(inID)
{
	Super::SetAxisMode(AxisModeExpand);
	if(inParent) {
		inParent->Parent(this);
	}
}

void UI::Centered::OnParented(Widget* inParent) {
	Super::SetAxisMode(inParent->GetAxisMode());
}

bool UI::Centered::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<ParentEvent>()) {
		// Update size in Super
		Super::OnEvent(inEvent);

		if(auto* child = Super::GetChild()) {
			// Pass size constrains to the child
			child->OnEvent(inEvent);

			// Now the child has updated it's size and we can set it's position
			const auto childSize = child->GetSize();
			const auto position = (event->Constraints - childSize) * 0.5f;
			child->SetPos(position);
		}
		return true;

	} else if(auto* event = inEvent->As<ChildEvent>()) {
		// Update size in Super
		Super::OnEvent(inEvent);
		
		const auto childSize = event->Child->GetSize();
		const auto position = (Super::GetSize() - childSize) * 0.5f;
		event->Child->SetPos(position);

		return true;
	}
	return Super::OnEvent(inEvent);
}



/*-------------------------------------------------------------------------------------------------*/
//										EXPANDED
/*-------------------------------------------------------------------------------------------------*/
UI::Expanded::Expanded(Widget* inParent, const std::string& inID)
	: Super(inID)
{
	if(inParent) {
		inParent->Parent(this);
	}
}

AxisIndex GetMainAxisIdx(UI::Widget* inFlexbox) {
	const auto parentDirection = inFlexbox->As<UI::Flexbox>()->GetDirection();
	return parentDirection == UI::ContentDirection::Row ? AxisX : AxisY;
}

AxisIndex UI::Expanded::GetMainAxis() const {
	auto* parent = Super::GetParent();
	Assert(parent);
	
	const auto parentDirection = parent->As<UI::Flexbox>()->GetDirection();
	return parentDirection == UI::ContentDirection::Row ? AxisX : AxisY;	
}

void UI::Expanded::OnParented(Widget* inParent) {
	Super::OnParented(inParent);
	Assertm(inParent->IsA<Flexbox>(), "Expanded parent can be only a Flexbox widget");
	auto parent = inParent->As<Flexbox>();
	Super::SetAxisMode(inParent->GetAxisMode());
}

bool UI::Expanded::OnEvent(IEvent* inEvent) {
	// Layout event will be handled in Super based on AxisMode

	if(auto* event = inEvent->As<ParentEvent>()) {
		Super::OnEvent(inEvent);
		
		if(auto child = Super::GetChild()) {
			ParentEvent onParent(*event);
			onParent.bForceExpand[GetMainAxis()] = true;
			child->OnEvent(&onParent);
		}
		return true;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		event->DrawList->DrawRectFilled(Super::GetRect().Translate(event->ParentOriginGlobal), Colors::SecondaryDark);
		Super::OnEvent(inEvent);
	}
	return Super::OnEvent(inEvent);
}





/*-------------------------------------------------------------------------------------------------*/
//										TEXT
/*-------------------------------------------------------------------------------------------------*/
UI::Text::Text(Widget* inParent, const std::string& inText, const std::string& inID) 
	: Super(inID)
{
	SetText(inText);
	if(inParent) {
		inParent->Parent(this);
	}
}

void UI::Text::SetText(const std::string& inText) {
	m_Text = inText;
	const auto* style = UI::GetDefaultTheme()->DefaultTextStyle.get();
	Assertf(style, "Widget type and style type mismatch");

	float2 size;

	if(inText.empty()) {
		/// TODO handle the case when the text is empty
		/// Maybe do not update parent
		/// For now we set it to some random values
		size = float2(15, 15);
	} else {
		size = style->CalculateTextSize(m_Text);
		Super::SetSize(size);
	}
	Super::NotifyParentOnSizeChanged(AxisX);
}

bool UI::Text::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<HoverEvent>()) {
		// Ignore, we cannot be hovered
		return false;

	} else if(auto* event = inEvent->As<ParentEvent>()) {
		// Ignore, our size depends only on text
		return false;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		event->DrawList->DrawText(event->ParentOriginGlobal + Super::GetOriginLocal(), Color("#dddddd"), m_Text, 13);
		return true;
	}
	return Super::OnEvent(inEvent);
}



/*-------------------------------------------------------------------------------------------------*/
//										FLEXBOX
/*-------------------------------------------------------------------------------------------------*/

UI::Flexbox::Flexbox(Widget* inParent, const FlexboxDesc& inDesc)
	: Super(inDesc.ID)
	, m_Direction(inDesc.Direction)
	, m_JustifyContent(inDesc.JustifyContent)
	, m_Alignment(inDesc.Alignment)
	, m_OverflowPolicy(inDesc.OverflowPolicy)
{
	Assertm(inDesc.bExpandMainAxis || !inDesc.bExpandMainAxis && inDesc.JustifyContent == JustifyContent::Start, "If main axis is shrinked, justification has no effect");

	const auto mainAxis = m_Direction == ContentDirection::Row ? 0 : 1;
	Super::SetAxisMode(mainAxis, inDesc.bExpandMainAxis ? AxisMode::Expand : AxisMode::Shrink);
	Super::SetAxisMode(!mainAxis, inDesc.bExpandCrossAxis ? AxisMode::Expand : AxisMode::Shrink);

	if(inParent) inParent->Parent(this);	
}

bool UI::Flexbox::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<ParentEvent>()) {
		Super::OnEvent(inEvent);
		UpdateLayout();
		return true;

	} else if(auto* event = inEvent->As<ChildEvent>()) {
		UpdateLayout();		
		Super::NotifyParentOnSizeChanged();
		return true;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		if(!Super::IsVisible()) return true;
		event->DrawList->DrawRect(Super::GetRect().Translate(event->ParentOriginGlobal), Colors::Blue);

		auto eventCopy = *event;
		eventCopy.ParentOriginGlobal += Super::GetOriginLocal();
		Super::DispatchToChildren(&eventCopy);
		return true;
	}
	return Super::OnEvent(inEvent);
}

void UI::Flexbox::Parent(Widget* inChild) {
	Super::Parent(inChild);
	const auto mainAxisMode = GetAxisMode()[GetMainAxisIdx(this)];
	Assertm(mainAxisMode == AxisMode::Expand || inChild->GetAxisMode()[mainAxisMode] == AxisMode::Shrink, 
			"Flexbox with shrinked main axis cannot have expanding children on that axis");
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
	const bool mainAxisIndex = bDirectionRow ? 0 : 1;
	const bool crossAxisIndex = bDirectionRow ? 1 : 0;

	const auto thisMainAxisSize = Super::GetSize()[mainAxisIndex];
	const auto thisCrossAxisSize = Super::GetSize()[crossAxisIndex];

	const auto axisMode = Super::GetAxisMode();

	// These options require extra space left on the main axis to work so we need to find it
	bool bJustifyContent = m_JustifyContent != JustifyContent::Start;

	std::vector<Widget*> visibleChildren;
	Super::GetVisibleChildren(&visibleChildren);

	std::vector<TempData> tempBuffer;
	tempBuffer.resize(visibleChildren.size());

	// Size of the biggest widget on the cross axis
	// Our size and sizes of expaned widgets should be this
	float maxChildSizeCrossAxis = 0.f;
	float totalFlexFactor = 0;
	float shrinkedChildrenSizeMainAxis = 0;

	// Find available space after fixed size widgets
	for(auto i = 0; i < visibleChildren.size(); ++i) {
		auto* child = visibleChildren[i];
		auto& temp = tempBuffer[i];

		// Main axis
		if(auto* flexible = child->As<Flexible>()) {
			Assertf(axisMode[mainAxisIndex] == AxisMode::Expand, "Flexbox main axis set to AxisMode::Shrink, but an expanded child is found");
			temp.MainAxisSize = -flexible->GetFlexFactor();
			totalFlexFactor += temp.MainAxisSize;

		} else if(auto* expanded = child->As<Expanded>()) {
			Assertf(axisMode[mainAxisIndex] == AxisMode::Expand, "Flexbox main axis set to AxisMode::Shrink, but an expanded child is found");
			temp.MainAxisSize = -1.f;
			totalFlexFactor += temp.MainAxisSize;

		} else {
			const auto childAxisMode = child->GetAxisMode()[mainAxisIndex];

			if(childAxisMode == AxisMode::Expand) {
				Assertf(axisMode[mainAxisIndex] == AxisMode::Expand, "Flexbox main axis set to AxisMode::Shrink, but an expanded child is found");
				temp.MainAxisSize = -1.f;
				totalFlexFactor += temp.MainAxisSize;

			} else {
				temp.MainAxisSize = child->GetSize()[mainAxisIndex];
				shrinkedChildrenSizeMainAxis += temp.MainAxisSize;
			}
		}

		// Cross axis
		const auto childAxisMode = child->GetAxisMode()[crossAxisIndex];

		if(childAxisMode == AxisMode::Expand) {
			temp.CrossAxisSize = thisCrossAxisSize;
		} else {
			temp.CrossAxisSize = child->GetSize()[crossAxisIndex];
		}		
		maxChildSizeCrossAxis = Math::Max(temp.CrossAxisSize, maxChildSizeCrossAxis);
	}
	float mainAxisRemainingSpace = thisMainAxisSize - shrinkedChildrenSizeMainAxis;

	// Check for overflow
	if(axisMode[mainAxisIndex] == AxisMode::Expand && mainAxisRemainingSpace <= 0.f) {

		if(m_OverflowPolicy == OverflowPolicy::Clip) {
			// Just clip overflown children
		} else if(m_OverflowPolicy == OverflowPolicy::Wrap) {
			// Check if can expand in cross axis and put children there
		} else {
			// Set up scrolling
		}
	}

	// Calculate sizes for flexible widgets if possible
	if(totalFlexFactor < 0.f) {
		bJustifyContent = false;
		
		for(auto i = 0; i < visibleChildren.size(); ++i) {
			auto& temp = tempBuffer[i];

			if(temp.MainAxisSize < 0.f) {
				temp.MainAxisSize = mainAxisRemainingSpace * temp.MainAxisSize / totalFlexFactor;
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
				justifyContentMargin = mainAxisRemainingSpace;
				break;
			}
			case JustifyContent::Center:
			{
				justifyContentMargin = mainAxisRemainingSpace * 0.5f;
				break;
			}
			case JustifyContent::SpaceBetween:
			{
				if(tempBuffer.size() == 1) {
					justifyContentMargin = mainAxisRemainingSpace * 0.5f;
				} else {
					justifyContentMargin = mainAxisRemainingSpace / (tempBuffer.size() - 1);
				}
				break;
			}
			case JustifyContent::SpaceAround:
			{
				if(tempBuffer.size() == 1) {
					justifyContentMargin = mainAxisRemainingSpace * 0.5f;
				} else {
					justifyContentMargin = mainAxisRemainingSpace / (tempBuffer.size() + 1);
				}
				break;
			}
		}
	}

	// Lay out children
	float mainAxisCursor = 0.f;

	if(m_JustifyContent == JustifyContent::Center ||
		m_JustifyContent == JustifyContent::End || m_JustifyContent == JustifyContent::SpaceAround) {
		mainAxisCursor = justifyContentMargin;
	}

	for(auto i = 0; i != tempBuffer.size(); ++i) {
		auto& temp = tempBuffer[i];
		temp.MainAxisPos = mainAxisCursor;
		mainAxisCursor += temp.MainAxisSize;

		if(bJustifyContent && m_JustifyContent == JustifyContent::SpaceBetween || m_JustifyContent == JustifyContent::SpaceAround) {
			mainAxisCursor += justifyContentMargin;
		}

		if(m_Alignment == AlignContent::End) {
			temp.CrossAxisPos = maxChildSizeCrossAxis - temp.CrossAxisSize;

		} else if(m_Alignment == AlignContent::Center) {
			temp.CrossAxisPos = 0.5f * (maxChildSizeCrossAxis - temp.CrossAxisSize);
		}

		float2 childPos;
		childPos[mainAxisIndex] = temp.MainAxisPos;
		childPos[crossAxisIndex] = temp.CrossAxisPos;

		ParentEvent layoutEvent;
		layoutEvent.Constraints[mainAxisIndex] = temp.MainAxisSize;
		layoutEvent.Constraints[crossAxisIndex] = temp.CrossAxisSize;

		visibleChildren[i]->OnEvent(&layoutEvent);
		visibleChildren[i]->SetPos(childPos);
	}

	if(axisMode[crossAxisIndex] == AxisMode::Shrink) {
		Super::SetSize(crossAxisIndex, maxChildSizeCrossAxis);
	}

	if(axisMode[mainAxisIndex] == AxisMode::Shrink) {
		Super::SetSize(mainAxisIndex, shrinkedChildrenSizeMainAxis);
	}
}



/*-------------------------------------------------------------------------------------------------*/
//										BUTTON
/*-------------------------------------------------------------------------------------------------*/
constexpr auto ButtonPaddings = float2(10.f, 10.f);

UI::Button::Button(Widget* inParent, OnPressedFunc inCallback, const std::string& inID)
	: Super(inID)
	, m_Callback(inCallback) {
	if(inParent) {
		inParent->Parent(this);
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
			m_State = ButtonState::Default;
		}
		return true;

	} if(auto* event = inEvent->As<MouseButtonEvent>()) {

		if(event->Button == MouseButtonEnum::ButtonLeft) {

			if(event->bButtonPressed) {
				m_State = ButtonState::Pressed;
			} else {
				m_State = ButtonState::Default;
			}

			if(m_Callback) {
				m_Callback(event->bButtonPressed);
			}
			return true;
		}
		return false;

	} else if(auto* event = inEvent->As<HitTestEvent>()) {
		auto hitPosLocalSpace = event->GetLastHitPos();

		if(Super::GetRect().Contains(hitPosLocalSpace)) {
			event->PushItem(this, hitPosLocalSpace - Super::GetOriginLocal());
			Super::DispatchToChildren(inEvent);
			return true;
		}
		return false;

	} else if(auto* event = inEvent->As<ChildEvent>()) {
		auto thisSize = event->Child->GetSize() + ButtonPaddings * 2.f;
		auto childPosCentered = ButtonPaddings;

		Super::SetSize(thisSize);
		event->Child->SetPos(childPosCentered);

		Super::NotifyParentOnSizeChanged(AxisX);
		return true;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		auto color = m_State == ButtonState::Hovered ? Colors::HoveredDark : m_State == ButtonState::Pressed ? Colors::PressedDark : Colors::PrimaryDark;
		event->DrawList->DrawRectFilled(Super::GetRect().Translate(event->ParentOriginGlobal), color);

		auto eventCopy = *event;
		eventCopy.ParentOriginGlobal += Super::GetOriginLocal();
		Super::DispatchToChildren(&eventCopy);
		return true;
	}
	return Super::OnEvent(inEvent);
}



/*-------------------------------------------------------------------------------------------------*/
//										SPLITBOX
/*-------------------------------------------------------------------------------------------------*/
UI::Guideline::Guideline(Widget* inParent, bool bIsVertical /*= true*/, OnDraggedFunc inCallback /*= {}*/, const std::string& inID /*= {}*/) 
	: Super(inID)
	, m_MainAxis(bIsVertical ? AxisY : AxisX)
	, m_State(ButtonState::Default)
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
			m_State = ButtonState::Default;
		}
		return true;

	} else if(auto* event = inEvent->As<ParentEvent>()) {
		float2 size;
		size[m_MainAxis] = event->Constraints[m_MainAxis];
		size[!m_MainAxis] = 5.f;
		Super::SetSize(size);
		return true;

	} else if(auto* event = inEvent->As<MouseButtonEvent>()) {

		if(event->Button == MouseButtonEnum::ButtonLeft) {

			if(event->bButtonPressed) {
				m_State = ButtonState::Pressed;
			} else {
				m_State = ButtonState::Default;
			}
			return true;
		}
		return false;

	} else if(auto* event = inEvent->As<HitTestEvent>()) {
		auto hitPosLocalSpace = event->GetLastHitPos();

		if(Super::GetRect().Contains(hitPosLocalSpace)) {
			event->PushItem(this, hitPosLocalSpace - Super::GetOriginLocal());
			return true;
		}
		return false;

	} else if(auto* event = inEvent->As<MouseDragEvent>()) {
		
		if(m_State == ButtonState::Pressed) {
			const auto delta = event->CursorDelta[!m_MainAxis];
			if(m_Callback) m_Callback(event);
		}

	} else if(auto * event = inEvent->As<DrawEvent>()) {
		if(!Super::IsVisible()) return true;

		auto color = m_State == ButtonState::Hovered ? Colors::HoveredDark : m_State == ButtonState::Pressed ? Colors::PressedDark : Colors::PrimaryDark;
		event->DrawList->DrawRectFilled(Super::GetRect().Translate(event->ParentOriginGlobal), color);
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

	ChildEvent onChild;
	onChild.Child = inChild;
	onChild.Size = inChild->GetSize();
	onChild.Subtype = ChildEvent::OnAdded;
	this->OnEvent(&onChild);
}

void UI::SplitBox::Unparent(Widget* inChild) {
	Assert(inChild && inChild == m_First.get() || inChild == m_Second.get());

	if(inChild == m_First.get()) {
		m_First.release();
	} else {
		m_Second.release();
	}
	ChildEvent onChild;
	onChild.Child = inChild;
	onChild.Size = inChild->GetSize();
	onChild.Subtype = ChildEvent::OnRemoved;
	this->OnEvent(&onChild);
}

bool UI::SplitBox::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->As<ParentEvent>()) {
		Super::OnEvent(inEvent);
		UpdateLayout();
		return true;

	} else if(auto* event = inEvent->As<ChildEvent>()) {
		UpdateLayout();
		return true;

	} else if(auto* event = inEvent->As<DrawEvent>()) {
		event->DrawList->DrawRect(Super::GetRect().Translate(event->ParentOriginGlobal), Colors::Green);

		auto eventCopy = *event;
		eventCopy.ParentOriginGlobal += Super::GetOriginLocal();
		DispatchToChildren(&eventCopy);
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

void UI::SplitBox::OnSeparatorDragged(MouseDragEvent* inDragEvent) {
	const auto mainAxisSize = Super::GetSize(m_MainAxis);
	const auto clampedPos = Math::Clamp(inDragEvent->CursorPosLocalSpace[m_MainAxis], 10.f, mainAxisSize - 10.f);
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

	if(!m_First && !m_Second) return;

	if(!m_First || !m_First->IsVisible()) {	
		ParentEvent onParent(Super::GetSize());
		m_Second->OnEvent(&onParent);
		m_Second->SetPos(float2(0.f));
		m_Separator->SetVisibility(false);

	} else if(!m_Second || !m_Second->IsVisible()) {
		ParentEvent onParent(Super::GetSize());
		m_First->OnEvent(&onParent);
		m_First->SetPos(float2(0.f));
		m_Separator->SetVisibility(false);

	} else {		
		m_Separator->SetVisibility(true);

		const auto mainAxisSize = Super::GetSize(m_MainAxis);
		const auto crossAxisSize = Super::GetSize(!m_MainAxis);		
		const auto separatorThickness = m_Separator->GetSize(m_MainAxis);
		const auto firstChildConstraint = m_SplitRatio * mainAxisSize - separatorThickness;
		const auto secondChildConstraint = mainAxisSize - separatorThickness - firstChildConstraint;

		float2 childConstraints;
		{
			childConstraints[m_MainAxis] = firstChildConstraint;
			childConstraints[!m_MainAxis] = crossAxisSize;

			ParentEvent onParent(childConstraints);
			m_First->OnEvent(&onParent);
			m_First->SetPos(float2(0.f));
		}
		{
			ParentEvent onParent(GetSize());
			m_Separator->OnEvent(&onParent);

			float2 pos;
			pos[m_MainAxis] = firstChildConstraint;
			m_Separator->SetPos(pos);
		}
		{
			childConstraints[m_MainAxis] = secondChildConstraint;
			childConstraints[!m_MainAxis] = crossAxisSize;

			ParentEvent onParent(childConstraints);
			m_Second->OnEvent(&onParent);

			float2 secondChildPos;
			secondChildPos[m_MainAxis] = firstChildConstraint + separatorThickness;
			m_Second->SetPos(secondChildPos);
		}
	}
}
