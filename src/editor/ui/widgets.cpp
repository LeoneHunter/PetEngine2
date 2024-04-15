#include "widgets.h"
#include "runtime/system/renderer/ui_renderer.h"

namespace ui {

/*-------------------------------------------------------------------------------------------------*/
//										TEXT
/*-------------------------------------------------------------------------------------------------*/
ui::TextBox::TextBox(const std::string& text, StringID style)
	: Super(Application::Get()->GetTheme()->Find(style)->Find<LayoutStyle>()) 
	, style_(Application::Get()->GetTheme()->Find(style))
	, text_(text) {
	// const auto size = style_->Find<TextStyle>()->CalculateTextSize(text_);
	// SetSize(size);
	SetAxisMode(SizeMode::Fixed());
}

bool ui::TextBox::OnEvent(IEvent* event) {
	if(auto* drawEvent = event->As<DrawEvent>()) {
		drawEvent->canvas->DrawText(GetOrigin(), style_->Find<TextStyle>(), text_);
		return true;
	}
	return Super::OnEvent(event);
}

float2 TextBox::OnLayout(const LayoutConstraints& event) {
	SetOrigin(event.rect.TL());
	const auto size = style_->Find<TextStyle>()->CalculateTextSize(text_);
	SetSize(size);
	const auto layoutInfo = *GetLayoutStyle();
	return GetSize() + layoutInfo.margins.Size();
}






}