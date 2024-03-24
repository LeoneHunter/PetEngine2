#include <iostream>
#include <ranges>

#include "runtime/core/types.h"
#include "runtime/system/job_dispatcher.h"
#include "editor/ui/ui.h"

using namespace UI;
UI::Application* g_Application = nullptr;

void SetDarkTheme() {
	auto* theme = new UI::Theme();
	{
		auto& s = theme->Add("Text");
		s.Add<TextStyle>().Color("#dddddd");
	}
	{
		auto& s = theme->Add("TitleBar");
		s.Add<LayoutStyle>().Margins(0).Paddings(5);
		s.Add<BoxStyle>().FillColor("#404040").Rounding(6);
	}
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
		s.Add<BoxStyle>().FillColor("#202020").Rounding(6).Borders(1).BorderColor("#aaaaaa");	

		theme->Add("ContextMenu", "Popup");
	}		
	{
		auto& s = theme->Add("Tooltip");
		s.Add<LayoutStyle>().Margins(5).Paddings(5);
		s.Add<BoxStyle>().FillColor("#303030").Rounding(4).Borders(1).BorderColor("#aaaaaa");
	}
	{
		auto& s = theme->Add("Transparent");
		s.Add<LayoutStyle>().Margins(0).Paddings(0);
		s.Add<BoxStyle>().FillColor("#ffffff").Rounding(0).Borders(0).Opacity(0.f);
	}
	{
		auto& s = theme->Add("OutlineRed");
		s.Add<LayoutStyle>().Margins(0).Paddings(0);
		s.Add<BoxStyle>().BorderColor("#ff0000").Rounding(0).Borders(1);
	}
	theme->Add("Flexbox").Add<LayoutStyle>().Margins(0).Paddings(0);
	g_Application->SetTheme(theme);
}

void TestFlexbox() {
	// g_Application->Parent(
	// 	Flexbox::Build()
	// 		.DirectionColumn()
	// 		.ID("FlexboxColumn")
	// 		.Style("TitleBar")
	// 		.Expand()
	// 		.JustifyContent(JustifyContent::Start)
	// 		.Children({Container::New(
	// 			ContainerFlags::HorizontalExpand | ContainerFlags::VerticalExpand | ContainerFlags::ClipVisibility,
	// 			"TitleBar",
	// 			Flexbox::Build()
	// 				.DirectionRow()
	// 				.ID("TitleBarFlexbox")
	// 				.Style("TitleBar")
	// 				.JustifyContent(JustifyContent::SpaceBetween)
	// 				.Children({
	// 					Text::New("Window title"),
	// 					Button::New("CloseButton", {}, Text::New("X"))
	// 				})
	// 				.New())
	// 		})
	// 		.New()
	// );
}

void TestFlexible() {
	g_Application->Parent(
		Flexbox::Build()
			.DirectionRow()
			.ID("FlexboxTest")
			.Style("")
			.Children({
				new Flexible(5, new Aligned(Alignment::Center, Alignment::Start, Button::Build().Text("Button 1").New())),
				new Flexible(5, new Aligned(Alignment::Center, Alignment::Start, Button::Build().Text("Button 2").New())),
				Button::Build().Text("Button 3").New()
			})
			.New());
}

void TestWindow() {
	g_Application->Parent(
		Window::Build()
			.ID("Window 1")
			.Position(300, 300)
			.Size(400, 400)
			.Style("FloatWindow")
			.Title("Window 1")
			.Popup([](const PopupOpenContext& ctx) {
				return new PopupWindow(nullptr, ctx.mousePosGlobal, float2(300, 0), 
					Button::Build().Text("My Button").SizeMode({AxisMode::Expand, AxisMode::Shrink}).New(),
					Button::Build().Text("My Button").SizeMode({AxisMode::Expand, AxisMode::Shrink}).New(),
					PopupMenuItemBuilder().Text("Submenu 1").Shortcut(">").New(),
					PopupMenuItemBuilder().Text("Menu item 1").Shortcut("Ctrl N").New());
			})
			.Children({
				Button::Build()
					.Text("My Button 1")
					.Tooltip("This is the button tooltip")
					.New()
			})
			.New(),
		Layer::Float
	);
}

int main(int argc, char* argv[]) {

	const auto commandLine = std::vector<std::string>(argv, argv + argc);
	auto logDir = std::filesystem::path(commandLine[0]).parent_path();
	logging::Init(logDir.string());
	logging::SetLevel(logging::Level::All);

	using namespace UI;

	g_Application = Application::Create("App", 1800, 900);

	SetDarkTheme();
	//TestFlexbox();
	//TestContainer();
	//TestFlexible();
	TestWindow();

	while(g_Application->Tick());
}
