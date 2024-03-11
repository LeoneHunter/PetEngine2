#include <iostream>

#include "runtime/core/types.h"
#include "runtime/system/job_dispatcher.h"
#include "editor/ui/ui.h"

using namespace UI;
UI::Application* g_Application = nullptr;

void SetDarkTheme() {
	auto* theme = new UI::Theme();
	{
		auto& buttonStyle = theme->Add("Button");
		buttonStyle.Add<LayoutStyle>("Normal").Margins(5).Paddings(5);
		buttonStyle.Add<LayoutStyle>("Hovered", "Normal");
		buttonStyle.Add<LayoutStyle>("Pressed", "Normal");

		buttonStyle.Add<BoxStyle>("Normal").FillColor("#505050").Borders(0).Rounding(6);
		buttonStyle.Add<BoxStyle>("Hovered", "Normal").FillColor("#707070");
		buttonStyle.Add<BoxStyle>("Pressed", "Normal").FillColor("#909090");
	}
	{
		auto& windowStyle = theme->Add("FloatWindow");
		windowStyle.Add<LayoutStyle>().Margins(5, 5).Paddings(5, 5);
		windowStyle.Add<BoxStyle>().FillColor("#303030").Rounding(6);		
	}
	{
		auto& windowStyle = theme->Add("Popup");
		windowStyle.Add<LayoutStyle>().Margins(5, 5).Paddings(5, 5);
		windowStyle.Add<BoxStyle>().FillColor("#202020").Rounding(6).Borders(1).BorderColor("#202020");	

		theme->Add("ContextMenu", "Popup");
	}		
	{
		auto& tooltip = theme->Add("Tooltip");
		tooltip.Add<LayoutStyle>().Margins(5).Paddings(5);
		tooltip.Add<BoxStyle>().FillColor("#303030").Rounding(4).Borders(1).BorderColor("#202020");
	}
	g_Application->SetTheme(theme);
}


int main(int argc, char* argv[]) {

	const auto commandLine = std::vector<std::string>(argv, argv + argc);

	using namespace UI;

	g_Application = Application::Create("App", 1800, 900);

	SetDarkTheme();

	g_Application->Parent(
		Window::Build()
			.ID("MyID")
			.StyleClass("FloatWindow")
			.Size(400, 400)
			.Position(300, 300)
			.New(),
		Layer::Float);

	while(g_Application->Tick());
}