#include "Widgets.h"
#include "UI.h"
#include "Runtime/System/Renderer/UIRenderer.h"


/*-------------------------------------------------------------------------------------------------*/
//										WINDOW
/*-------------------------------------------------------------------------------------------------*/
UI::Window* UI::WindowBuilder::Create(Application* inApp) {
	m_App = inApp;
	return new Window(*this);
}

std::unique_ptr<UI::Window> UI::WindowBuilder::Create() {
	return std::make_unique<UI::Window>(*this);
}

UI::Window::Window(Application* inApp, const std::string& inID, WindowFlags inFlags, const std::string& inStyleClassName)
	: Super(inID) 
	, m_Flags(inFlags)
	, m_Style(Application::GetFrameState().Theme->Find(inStyleClassName))
{
	SetAxisMode(AxisModeExpand);
	
	if(m_Flags & WindowFlags::ShrinkToFit) {
		SetAxisMode(AxisModeShrink);
	}

	if(inApp) inApp->ParentWindow(this);
}

Point UI::Window::TransformLocalToGlobal(Point inPosition) const {
	return inPosition + GetOrigin();
}

UI::Window::Window(const WindowBuilder& inBuilder) 
	: Super(inBuilder.m_ID)
	, m_Flags(inBuilder.m_Flags)
	, m_Style(UI::Application::Get()->GetTheme()->Find(inBuilder.m_StyleClass))
{
	SetAxisMode(AxisModeExpand);
	
	if(m_Flags & WindowFlags::ShrinkToFit) {
		SetAxisMode(AxisModeShrink);
	}
	SetOrigin(inBuilder.m_Pos);
	SetSize(inBuilder.m_Size);

	if(inBuilder.m_App)
		inBuilder.m_App->ParentWindow(this);
}

bool UI::Window::OnEvent(IEvent* inEvent) {

	if(auto* mouseMoveEvent = inEvent->Cast<MouseDragEvent>()) {
		SetOrigin(GetOrigin() + mouseMoveEvent->MouseDelta);
		return true;
	}

	if(auto* mouseButtonEvent = inEvent->Cast<MouseButtonEvent>()) {

		if(mouseButtonEvent->Button == MouseButton::ButtonLeft) {
			return true;
		}
		return false;
	}

	if(auto* hoverEvent = inEvent->Cast<HoverEvent>()) {
		auto bHandledByChildren = DispatchToChildren(inEvent);

		if(!bHandledByChildren) {
			return true;
		}
		return bHandledByChildren;
	}
		
	if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {

		if(m_Flags & WindowFlags::Background) {
			ExpandToParent(event);	
			DispatchLayoutToChild(event);
		}
		return true;
	}

	if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
		HandleChildEvent(event);
		return true;
	}
		
	if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
		drawEvent->DrawList->PushBox(GetRect(), m_Style->Find<BoxStyle>());
		drawEvent->DrawList->PushClipRect(GetRect());

		DispatchDrawToChild(drawEvent);
		drawEvent->DrawList->PopClipRect();
		return true;
	}

	return Super::OnEvent(inEvent);
}

UI::LayoutInfo UI::Window::GetLayoutInfo() const {
	if(!m_Style) return {};
	const auto* layoutStyle = m_Style->Find<LayoutStyle>();
	return layoutStyle ? LayoutInfo{layoutStyle->Margins, layoutStyle->Paddings} : LayoutInfo{};
}




/*-------------------------------------------------------------------------------------------------*/
//										CENTERED
/*-------------------------------------------------------------------------------------------------*/
UI::Centered::Centered(WidgetAttachSlot& inSlot, const std::string& inID)
	: Super(inID)
{
	SetAxisMode(AxisModeExpand);
	inSlot.Parent(this);
}

void UI::Centered::OnParented(Widget* inParent) {
	Super::OnParented(inParent);
	auto layoutParent = GetParent<LayoutWidget>();	
	SetAxisMode(layoutParent->GetAxisMode());
}

bool UI::Centered::OnEvent(IEvent* inEvent) {

	if(auto* layoutEvent = inEvent->Cast<ParentLayoutEvent>()) {
		const auto bSizeChanged = ExpandToParent(layoutEvent);

		if(bSizeChanged) {
			NotifyChildOnSizeChanged();
			CenterChild(layoutEvent);
		}		
		return true;
	}
	
	if(auto* childEvent = inEvent->Cast<ChildLayoutEvent>()) {
		HandleChildEvent(childEvent);
		CenterChild(childEvent);
		return true;
	}
	return Super::OnEvent(inEvent);
}



/*-------------------------------------------------------------------------------------------------*/
//										FLEXBOX
/*-------------------------------------------------------------------------------------------------*/
UI::Flexbox* UI::FlexboxBuilder::Create(WidgetAttachSlot& inSlot) {	
	SetParentSlot(inSlot);
	return new Flexbox(*this);
}

UI::Flexbox::Flexbox(const FlexboxBuilder& inBuilder)
	: Super(inBuilder.m_ID)
	, m_Direction(inBuilder.m_Direction)
	, m_JustifyContent(inBuilder.m_JustifyContent)
	, m_Alignment(inBuilder.m_Alignment)
	, m_OverflowPolicy(inBuilder.m_OverflowPolicy)
{
	const auto mainAxis = m_Direction == ContentDirection::Row ? 0 : 1;
	SetAxisMode(mainAxis, inBuilder.m_ExpandMainAxis ? AxisMode::Expand : AxisMode::Shrink);
	SetAxisMode(!mainAxis, inBuilder.m_ExpandCrossAxis ? AxisMode::Expand : AxisMode::Shrink);

	if(inBuilder.m_Slot) inBuilder.m_Slot->Parent(this);
}

bool UI::Flexbox::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
		// Update our size if expanded
		ExpandToParent(event);
		UpdateLayout();
		return true;
	} 
	
	if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {

		if(event->Subtype == ChildLayoutEvent::OnAdded) {
			const auto crossAxis = m_Direction == ContentDirection::Row ? AxisY : AxisX;
			Assertf(GetAxisMode()[crossAxis] == AxisMode::Expand || event->Child->GetAxisMode()[crossAxis] == AxisMode::Shrink,
								"Parent child axis shrink/expand mismatch. A child with an expanded axis has been added to the containter with that axis shrinked."
								"Parent {}, Child {}", GetDebugIDString(), event->Child->GetDebugIDString());
		}
		UpdateLayout();		
		NotifyParentOnSizeChanged();
		return true;
	} 
	
	if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
		//drawEvent->DrawList->PushBox(GetRect(), Colors::Blue, false);
		drawEvent->DrawList->PushClipRect(GetRect());
		drawEvent->DrawList->PushTransform(GetOrigin());

		DispatchToChildren(drawEvent);

		drawEvent->DrawList->PopTransform();
		drawEvent->DrawList->PopClipRect();
		return true;
	}

	return Super::OnEvent(inEvent);
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
	auto parent = GetParent();
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

	const auto paddings = GetLayoutInfo().Paddings;
	const auto innerSize = GetSize() - paddings.Size();
	const auto innerMainAxisSize = innerSize[mainAxisIndex];
	const auto innerCrossAxisSize = innerSize[crossAxisIndex];

	const auto axisMode = GetAxisMode();

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
	auto mainAxisCursor = (float)paddings.TL()[mainAxisIndex];

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
		SetSize(crossAxisIndex, maxChildSizeCrossAxis);
	}

	if(axisMode[mainAxisIndex] == AxisMode::Shrink || m_OverflowPolicy == OverflowPolicy::ShrinkWrap && totalMainAxisContentSize > innerMainAxisSize) {
		SetSize(mainAxisIndex, totalMainAxisContentSize);
	}
}



/*-------------------------------------------------------------------------------------------------*/
//										BUTTON
/*-------------------------------------------------------------------------------------------------*/
UI::Button* UI::ButtonBuilder::Create(WidgetAttachSlot& inSlot) {
	SetParentSlot(inSlot);
	return new UI::Button(*this);
}

UI::Button::Button(WidgetAttachSlot& inSlot, OnPressedFunc inCallback, const std::string& inID)
	: Super(inID)
	, m_Style(Application::Get()->GetTheme()->Find(GetClassName().data()))
	, m_State(State::Normal)
	, m_Callback(inCallback)
{
	inSlot.Parent(this);
}

UI::Button::Button(const ButtonBuilder& inBuilder)
	: Super(inBuilder.m_ID)
	, m_Style(Application::Get()->GetTheme()->Find(GetClassName().data()))
	, m_State(State::Normal)
	, m_Callback(inBuilder.m_Callback)
{
	DefaultSlot = inBuilder.m_Child.release();

	if(inBuilder.m_Slot)
		inBuilder.m_Slot->Parent(this);	
}

bool UI::Button::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->Cast<HoverEvent>()) {
		
		// Change state and style
		if(event->bHovered) {
			m_State = State::Hovered;
		} else {
			m_State = State::Normal;
		}
		return true;
	} 
	
	if(auto* event = inEvent->Cast<MouseButtonEvent>()) {

		if(event->Button == MouseButton::ButtonLeft) {

			if(event->bButtonPressed) {
				m_State = State::Pressed;
			} else {
				m_State = State::Normal;
			}

			if(m_Callback) {
				m_Callback(event->bButtonPressed);
			}
			return true;
		}
		return false;
	} 
	
	if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
		HandleChildEvent(event);
		NotifyParentOnSizeChanged();
		return true;
	} 
	
	if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
		drawEvent->DrawList->PushBox(GetRect(), m_Style->Find<BoxStyle>(m_State));
		DispatchDrawToChild(drawEvent);
		return true;
	}

	return Super::OnEvent(inEvent);
}

UI::LayoutInfo UI::Button::GetLayoutInfo() const {

	if(m_Style) {
		const auto* layoutStyle = m_Style->Find<LayoutStyle>(m_State);

		if(layoutStyle) {
			return {layoutStyle->Margins, layoutStyle->Paddings};
		}
	}
	return {};
}




/*-------------------------------------------------------------------------------------------------*/
//										TEXT
/*-------------------------------------------------------------------------------------------------*/
UI::Text::Text(const std::string& inText, const std::string& inID /*= {}*/)
	: Super(inID)
	, m_Style(Application::Get()->GetTheme()->Find(GetClassName().data()))
	, m_Text(inText) 
{
	const auto size = m_Style->Find<TextStyle>()->CalculateTextSize(m_Text);
	SetSize(size);
}

UI::Text::Text(WidgetAttachSlot& inSlot, const std::string& inText, const std::string& inID)
	: Text(inText, inID) {
	inSlot.Parent(this);
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
		SetSize(size);
	}
	NotifyParentOnSizeChanged();
}

bool UI::Text::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->Cast<HoverEvent>()) {
		// Ignore, we cannot be hovered
		return false;
	} 
	
	if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
		// Ignore size
		SetOrigin(event->Constraints.TL());
		return false;
	}
	
	if(auto* event = inEvent->Cast<DrawEvent>()) {
		event->DrawList->PushText(GetOrigin(), m_Style->Find<TextStyle>(), m_Text);
		return true;
	}

	return Super::OnEvent(inEvent);
}



/*-------------------------------------------------------------------------------------------------*/
//										GUIDELINE
/*-------------------------------------------------------------------------------------------------*/
UI::Guideline::Guideline(WidgetAttachSlot& inSlot, bool bIsVertical /*= true*/, OnDraggedFunc inCallback /*= {}*/, const std::string& inID /*= {}*/) 
	: Super(std::string(inID))
	, m_MainAxis(bIsVertical ? AxisY : AxisX)
	, m_State(State::Normal)
	, m_Callback(inCallback)
{
	inSlot.Parent(this);
}

bool UI::Guideline::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->Cast<HoverEvent>()) {
		// Change state and style
		if(event->bHovered) {
			m_State = State::Hovered;
		} else {
			m_State = State::Normal;
		}
		return true;
	} 
	
	if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
		float2 size;
		size[m_MainAxis] = event->Constraints.Size()[m_MainAxis];
		size[!m_MainAxis] = 10.f;
		SetSize(size);
		SetOrigin(event->Constraints.TL());
		return true;
	} 
	
	if(auto* event = inEvent->Cast<MouseButtonEvent>()) {

		if(event->Button == MouseButton::ButtonLeft) {

			if(event->bButtonPressed) {
				m_State = State::Pressed;
			} else {
				m_State = State::Normal;
			}
			return true;
		}
		return false;
	} 
	
	if(auto* event = inEvent->Cast<MouseDragEvent>()) {
		
		if(m_State == State::Pressed) {
			const auto delta = event->MouseDelta[!m_MainAxis];
			if(m_Callback) m_Callback(event);
		}
		return true;
	} 
	
	if(auto * event = inEvent->Cast<DrawEvent>()) {
		auto color = m_State == State::Hovered ? Colors::HoveredDark : m_State == State::Pressed ? Colors::PressedDark : Colors::PrimaryDark;
		event->DrawList->PushBox(GetRect(), color, true);
		return true;
	}
	return Super::OnEvent(inEvent);
}

void UI::Guideline::DebugSerialize(Debug::PropertyArchive& inArchive) {
	Super::DebugSerialize(inArchive);
	inArchive.PushProperty("MainAxis", !m_MainAxis ? "X" : "Y");
	inArchive.PushProperty("State", m_State.String());
}




/*-------------------------------------------------------------------------------------------------*/
//										SPLITBOX
/*-------------------------------------------------------------------------------------------------*/
UI::SplitBox::SplitBox(WidgetAttachSlot& inSlot, bool bHorizontal /*= true*/, const std::string& inID /*= {}*/) 
	: Super(inID)
	, m_MainAxis(bHorizontal ? AxisX : AxisY)
	, m_SplitRatio(0.5f)
	, FirstSlot(this)
	, SecondSlot(this)
	, m_Separator(this)
{
	new Guideline(
		m_Separator,
		bHorizontal,
		[this](auto* inDragEvent) { OnSeparatorDragged(inDragEvent); },
		std::string(*GetID()).append("::Separator"));

	SetAxisMode(AxisModeExpand);
	inSlot.Parent(this);
}

bool UI::SplitBox::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->Cast<ParentLayoutEvent>()) {
		ExpandToParent(event);
		UpdateLayout();
		return true;
	} 
	
	if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
		UpdateLayout();
		return true;
	}
	
	if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
		drawEvent->DrawList->PushBox(GetRect(), Colors::Green, false);
		drawEvent->DrawList->PushTransform(GetOrigin());
		DispatchToChildren(inEvent);
		drawEvent->DrawList->PopTransform();
		return true;
	}
	return Super::OnEvent(inEvent);
}

bool UI::SplitBox::DispatchToChildren(IEvent* inEvent) {
	bool bBroadcast = inEvent->IsBroadcast();

	for(auto i = 0; i < 3; ++i) {
		auto* child = i == 0 ? FirstSlot.Get() : i == 1 ? m_Separator.Get() : SecondSlot.Get();
		const auto bHandled = child->IsVisible() ? child->OnEvent(inEvent) : false;

		if(bHandled && !bBroadcast) {
			return true;
		}
	}
	return false;
}

void UI::SplitBox::DebugSerialize(Debug::PropertyArchive& inArchive) {
	Super::DebugSerialize(inArchive);
	inArchive.PushProperty("Direction", m_MainAxis ? "Column" : "Row");
	inArchive.PushProperty("SplitRatio", m_SplitRatio);
}

UI::VisitResult UI::SplitBox::VisitChildren(const WidgetVisitor& inVisitor, bool bRecursive) {
	for(auto i = 0; i < 3; ++i) {
		auto* child = i == 0 ? FirstSlot.Get() : i == 1 ? m_Separator.Get() : SecondSlot.Get();

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
	const auto clampedPos = Math::Clamp(inDragEvent->MousePosLocal[m_MainAxis], 10.f, mainAxisSize - 10.f);
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

	if(FirstSlot) {
		firstLayoutWidget = FirstSlot->Cast<LayoutWidget>();

		if(!firstLayoutWidget) {
			firstLayoutWidget = FirstSlot->GetChild<LayoutWidget>();
		}
	}

	LayoutWidget* secondLayoutWidget = nullptr;

	if(SecondSlot) {
		secondLayoutWidget = SecondSlot->Cast<LayoutWidget>();

		if(!secondLayoutWidget) {
			secondLayoutWidget = SecondSlot->GetChild<LayoutWidget>();
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
UI::Tooltip::Tooltip(Point inPos)
	: Window(
		WindowBuilder()
		.Position(inPos)
		.Flags(WindowFlags::Overlay)
		.StyleClass(GetClassName().data()))
{
	SetAxisMode(AxisModeShrink);
}

bool UI::Tooltip::OnEvent(IEvent* inEvent) {

	if(auto* event = inEvent->Cast<ChildLayoutEvent>()) {
		HandleChildEvent(event);
		NotifyParentOnSizeChanged();
		return true;
	}

	return Super::OnEvent(inEvent);
}

bool UI::TooltipSpawner::OnEvent(IEvent* inEvent) {
	constexpr unsigned delayMs = 1500;

	if(auto* hoverEvent = inEvent->Cast<HoverEvent>()) {

		if(hoverEvent->bHovered) {
			const auto bHandledByChild = Super::OnEvent(inEvent);

			if(bHandledByChild && s_State == State::Normal) {
				s_State = State::Waiting;

				const auto timerCallback = [this]()->bool {

					if(s_State == State::Waiting) {
						SpawnTooltipIntent event{this};
						DispatchToParent(&event);
					}
					return false;
				};
				Application::Get()->AddTimer(this, timerCallback, delayMs);
			}
			return bHandledByChild;

		} else {

			if(s_State == State::Active) {
				CloseTooltip();

			} else if(s_State == State::Disabled || s_State == State::Waiting) {
				s_State = State::Normal;
			}
			return Super::OnEvent(inEvent);
		}
	}

	if(auto* mouseButtonEvent = inEvent->Cast<MouseButtonEvent>()) {
		CloseTooltip();
		s_State = State::Disabled;
		return Super::OnEvent(inEvent);
	}

	return Super::OnEvent(inEvent);
}

std::unique_ptr<UI::Tooltip> UI::TooltipSpawner::OnSpawn(Point inMousePosGlobal) {
	auto out = m_Spawner(inMousePosGlobal);
	s_State = State::Active;
	s_Tooltip = out.get();
	return out;
}

void UI::TooltipSpawner::CloseTooltip() {

	if(s_State == State::Active) {
		CloseTooltipIntent closeTooltip(s_Tooltip);
		DispatchToParent(&closeTooltip);
		s_Tooltip = nullptr;
		s_State = State::Normal;
	}	
}



/*-------------------------------------------------------------------------------------------------*/
//										POPUP
/*-------------------------------------------------------------------------------------------------*/
std::unique_ptr<UI::PopupWindow> UI::PopupBuilder::Create() {
	return std::make_unique<UI::PopupWindow>(*this);
}

UI::PopupWindow::PopupWindow(const std::string& inID)
	: Window(nullptr, inID, WindowFlags::Popup)
	, m_Spawner(nullptr)
	, m_NextPopup(this)
	, m_NextPopupSpawner(nullptr)
{}

UI::PopupWindow::~PopupWindow() {
	m_Spawner->OnPopupDestroyed();
}

UI::PopupWindow::PopupWindow(const PopupBuilder& inBuilder)
	: Window(
		WindowBuilder()
		.Position(inBuilder.m_Pos)
		.Flags(WindowFlags::Popup)
		.Size(inBuilder.m_Size)
		.ID(inBuilder.m_ID)
		.StyleClass(inBuilder.m_StyleClass))
	, m_Spawner(nullptr)
	, m_NextPopup(this)
	, m_NextPopupSpawner(nullptr) 
{}

bool UI::PopupWindow::OnEvent(IEvent* inEvent) {

	if(auto* mouseButtonEvent = inEvent->Cast<MouseButtonEvent>()) {

		// Close if clicked outside the window
		if(!GetRect().Contains(mouseButtonEvent->MousePosGlobal)) {
			ClosePopupIntent closePopup(this);
			DispatchToParent(&closePopup);

			return true;
		}
		return false;
	}

	if(auto* hoverEvent = inEvent->Cast<HoverEvent>()) {

		if(m_NextPopup && m_NextPopup->OnEvent(inEvent)) {
			return false;
		}

		auto bHandledByChildren = DispatchToChildren(inEvent);

		if(!bHandledByChildren && !GetParent<PopupWindow>()) {
			return true;
		}
		return bHandledByChildren;
	}

	if(auto* hitTest = inEvent->Cast<HitTestEvent>()) {
		
		if(m_NextPopup && m_NextPopup->OnEvent(inEvent)) {
			return true;
		}
		auto hitPosLocalSpace = hitTest->GetLastHitPos();
		const auto bThisHovered = GetRect().Contains(hitPosLocalSpace);

		if(!GetParent<PopupWindow>() || bThisHovered) {
			hitTest->PushItem(this, hitPosLocalSpace - GetOrigin());
		}
		
		if(bThisHovered) {
			DispatchToChildren(inEvent);
		}
		// Because we are a Popup, block this event for propagating 
		// even if we aren't hovered
		return !GetParent<PopupWindow>() || bThisHovered;
	}

	if(auto* spawnPopupIntent = inEvent->Cast<SpawnPopupIntent>()) {
		const auto ctx = Application::GetFrameState();
		m_NextPopup = spawnPopupIntent->Spawner->OnSpawn(ctx.MousePosGlobal, ctx.WindowSize);
		m_NextPopup->OnParented(this);
		m_NextPopupSpawner = spawnPopupIntent->Spawner;
		return true;
	}

	if(auto* closePopupIntent = inEvent->Cast<ClosePopupIntent>()) {
		// Close all popup stack 
		ClosePopupIntent closePopup(this);
		DispatchToParent(&closePopup);
		return true;
	}

	if(auto* layoutEvent = inEvent->Cast<ParentLayoutEvent>()) {
		const auto viewportSize = layoutEvent->Constraints.Size();
		const auto popupRect = GetRect();
			  auto newOrigin = GetOrigin();

		for(auto axis = 0; axis < 2; ++axis) {
			if(popupRect.max[axis] > viewportSize[axis]) {
				newOrigin[axis] = viewportSize[axis] - popupRect.Size()[axis];
			}
		}
		SetOrigin(newOrigin);
		return true;
	}

	if(auto* drawEvent = inEvent->Cast<DrawEvent>()) {
		Super::OnEvent(inEvent);
		if(m_NextPopup) m_NextPopup->OnEvent(inEvent);
		return true;
	}

	return Super::OnEvent(inEvent);
}

void UI::PopupWindow::OnPopupItemHovered(PopupItem* inPopupItem) {

}

void UI::PopupWindow::OnPopupItemPressed(PopupItem* inPopupItem) {

}


