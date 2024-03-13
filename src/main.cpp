#include <iostream>

#include "runtime/core/types.h"
#include "runtime/system/job_dispatcher.h"
#include "editor/ui/ui.h"

using namespace UI;
UI::Application* g_Application = nullptr;

void SetDarkTheme() {
	auto* theme = new UI::Theme();
	{
		auto& s = theme->Add("Button");
		s.Add<LayoutStyle>("Normal").Margins(5).Paddings(5);
		s.Add<LayoutStyle>("Hovered", "Normal");
		s.Add<LayoutStyle>("Pressed", "Normal");

		s.Add<BoxStyle>("Normal").FillColor("#505050").Borders(0).Rounding(6);
		s.Add<BoxStyle>("Hovered", "Normal").FillColor("#707070");
		s.Add<BoxStyle>("Pressed", "Normal").FillColor("#909090");
	}
	{
		auto& s = theme->Add("FloatWindow");
		s.Add<LayoutStyle>().Margins(5, 5).Paddings(0);
		s.Add<BoxStyle>().FillColor("#303030").Rounding(6);		
		s.Add<BoxStyle>("Hovered").FillColor("#606060");		
	}
	{
		auto& s = theme->Add("Popup");
		s.Add<LayoutStyle>().Margins(5, 5).Paddings(5, 5);
		s.Add<BoxStyle>().FillColor("#202020").Rounding(6).Borders(1).BorderColor("#202020");	

		theme->Add("ContextMenu", "Popup");
	}		
	{
		auto& s = theme->Add("Tooltip");
		s.Add<LayoutStyle>().Margins(5).Paddings(5);
		s.Add<BoxStyle>().FillColor("#303030").Rounding(4).Borders(1).BorderColor("#202020");
	}
	theme->Add("Flexbox").Add<LayoutStyle>().Margins(0).Paddings(0);
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
			.Style("FloatWindow")
			.Size(400, 400)
			.Position(300, 300)
			.New(),
		Layer::Float);

	while(g_Application->Tick());
}