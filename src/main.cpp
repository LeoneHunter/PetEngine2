#include <iostream>

#include "runtime/core/types.h"
#include "runtime/system/job_dispatcher.h"
#include "editor/ui/ui.h"

using namespace UI;
UI::Application* g_Application = nullptr;

void SetDarkTheme() {
	auto* theme = new UI::Theme();
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

void TestFlexbox() {
	g_Application->Parent(
		Flexbox::Build()
			.DirectionColumn()
			.ID("FlexboxColumn")
			.Style("TitleBar")
			.Expand()
			.JustifyContent(JustifyContent::Start)
			.Children({Container::New(
				ContainerFlags::HorizontalExpand | ContainerFlags::VerticalExpand | ContainerFlags::ClipVisibility,
				"TitleBar",
				Flexbox::Build()
					.DirectionRow()
					.ID("TitleBarFlexbox")
					.Style("TitleBar")
					.JustifyContent(JustifyContent::SpaceBetween)
					.Children({
						Text::New("Window title"),
						Button::New("CloseButton", {}, Text::New("X"))
					})
					.New())
			})
			.New()
	);
}

void TestContainer() {
	g_Application->Parent(
		Container::NewFixed(
			ContainerFlags::ClipVisibility,
			float2(200, 200),
			"TitleBar",
			Button::New("CloseButton", {}, Text::New("X"))
		)
	);
}

void TestWindow() {
	g_Application->Parent(
		Window::Build()
			.ID("Window 1")
			.Style("FloatWindow")
			.Size(400, 400)
			.Position(300, 300)
			.Children({
				TooltipPortal::New(
					[](const TooltipBuildContext& inCtx) { 
						return Tooltip::NewText(std::format("Item ID: {}", inCtx.sourceWidget->FindChildOfClass<Button>()->GetDebugID())); 
					},
					Button::New(
						"Button",
						[](ButtonEvent* e) {
							if(!e->bPressed) {
								LOGF(Verbose, "Hello from button {}", e->source->GetDebugID());
							}
						},
						Text::New("Button 1")
					)
				),
				TooltipPortal::New(
					[](const TooltipBuildContext& inCtx) { 
						return Tooltip::NewText(std::format("Item ID: {}", inCtx.sourceWidget->FindChildOfClass<Button>()->GetDebugID())); 
					},
					Button::New(
						"Button",
						[](ButtonEvent* e) {
							if(!e->bPressed) {
								LOGF(Verbose, "Hello from button {}", e->source->GetDebugID());
							}
						},
						Text::New("Button 2 Long text")
					)
				)
			})
			.New(),
	Layer::Float);
	
	g_Application->Parent(
		Window::Build()
			.ID("Window 2")
			.Style("FloatWindow")
			.Size(400, 400)
			.Position(800, 300)
			.Children({
				TooltipPortal::New(
					[](const TooltipBuildContext& inCtx) { 
						return Tooltip::NewText(std::format("Item ID: {}", inCtx.sourceWidget->FindChildOfClass<Button>()->GetDebugID())); 
					},
					Button::New(
						"Button",
						[](ButtonEvent* e) {
							if(!e->bPressed) {
								LOGF(Verbose, "Hello from button {}", e->source->GetDebugID());
							}
						},
						Text::New("Button 3")
					)
				),
				TooltipPortal::New(
					[](const TooltipBuildContext& inCtx) { 
						return Tooltip::NewText(std::format("Item ID: {}", inCtx.sourceWidget->FindChildOfClass<Button>()->GetDebugID())); 
					},
					Button::New(
						"Button",
						[](ButtonEvent* e) {
							if(!e->bPressed) {
								LOGF(Verbose, "Hello from button {}", e->source->GetDebugID());
							}
						},
						Text::New("Button 4 Long text")
					)
				)
			})
			.New(),
	Layer::Float);
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
	TestWindow();

	while(g_Application->Tick());
}