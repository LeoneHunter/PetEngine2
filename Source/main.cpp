#include <iostream>

#include "Runtime/Core/BaseTypes.h"
#include "Runtime/System/JobDispatcher.h"
#include "Editor/UI/UI.h"

UI::Application* g_Application = nullptr;

void CreateTheme() {
	//using namespace UI;

	//auto theme = new UI::Theme();

	//// Create prototype
	//auto containerStyle = UI::StyleClass();
	//containerStyle.AddStyle("", LayoutStyle().SetMargins(0).SetPaddings(0));
	//containerStyle.AddStyle("", BoxStyle().SetFillColor("#454545").SetBorders(0).SetRounding(0));

	//// Set prototype style for multiple widgets
	//theme->AddStyleClass("Flexbox", containerStyle);
	//theme->AddStyleClass("Splitbox", containerStyle);

	//// Prototype for primitive types like button or tooltip
	//auto primitiveWidgetLayoutStyle = UI::LayoutStyle();
	//primitiveWidgetLayoutStyle.SetMargins(5, 5).SetPaddings(5, 7);

	//// Use prototype
	//auto textTooltipStyle = theme->AddStyleClass("TextTooltip");
	//textTooltipStyle.AddStyle("", primitiveWidgetLayoutStyle);
	//textTooltipStyle.AddStyle("", UI::TextStyle().SetColor("#454554"));


	//auto buttonStyle = theme->AddStyleClass("Button"); {
	//	buttonStyle.AddLayoutStyle("Normal").SetMargins(10).SetPaddings(5);
	//	buttonStyle.AddBoxStyle("Normal").SetFillColor("#454545").SetBorders(1.f).SetBorderColor("#343434");
	//	buttonStyle.AddBoxStyle("Hovered", "Normal").SetFillColor("#808080");
	//	buttonStyle.AddBoxStyle("Pressed", "Normal").SetFillColor("#eeeeee");
	//}
	//theme->AddStyleClass("Text").AddTextStyle("Normal").SetColor("#ffffff").SetSize(20);
	//
	//g_Application->SetTheme(theme);
}

void BuildSimple() {
	using namespace UI;

	auto* root = g_Application->GetRoot();

	auto centered = new Centered(root); {
		Button::TextButton(new TooltipSpawner(centered, []() { return Tooltip::Text("Tooltip"); }), "Button 1");
	}	
}

void BuildFlexboxOverflowTestLayout() {
	using namespace UI;

	auto* root = g_Application->GetRoot();

	/// Test expand and align on cross axis
	auto centered = new Centered(root); {

		auto splitBox = new SplitBox(centered, true, "Splitbox"); {

			auto row = Flexbox::Row().ID("Main Row").Parent(splitBox); {
				Button::TextButton(new TooltipSpawner(row, []() { return Tooltip::Text("Tooltip"); }), "Button 1");
				Button::TextButton(new TooltipSpawner(row, []() { return Tooltip::Text("Tooltip"); }), "Button 2");
			}

			auto placeholder = Button::TextButton(new TooltipSpawner(splitBox, []() { return Tooltip::Text("Tooltip"); }), "Button 1");
		}
	}	
}

void BuildAppScaffold() {
	using namespace UI;

	auto* root = g_Application->GetRoot(); {

		auto* centered = new Centered(root, "Root_Centered"); {

			auto* column = Flexbox::Build()
				.ID("Main_Column")
				.Direction(ContentDirection::Column)
				.OverflowPolicy(OverflowPolicy::Clip)
				.JustifyContent(JustifyContent::Center)
				.ExpandMainAxis(true)
				.ExpandCrossAxis(true)
				.Parent(centered); 
			{

				auto menuBar = Flexbox::Row().ID("Menu Bar").Parent(column); {

					auto leftGroup = Flexbox::Row().ID("Left_Group").Parent(menuBar); {
						Button::TextButton(leftGroup, "Menu Item 1");
						Button::TextButton(leftGroup, "Menu Item 2");
					}

					auto rightGroup = Flexbox::Row().ID("Right_Group").JustifyContent(UI::JustifyContent::End).Parent(menuBar); {
						Button::TextButton(rightGroup, "Menu Item 3");
						Button::TextButton(rightGroup, "Menu Item 4");
					}
				}

				auto middleRow = Flexbox::Row().ID("Main_Middle_Row").ExpandCrossAxis(true).Parent(column); {

					struct ToolbarController {

						void SetToolbarWindow(LayoutWidget* inWindow) {
							m_Toolbar = inWindow;
						}

						void OpenToolbar() {
							m_Toolbar->SetVisibility(true);
							m_Toolbar->NotifyParentOnVisibilityChanged();
						}

						void CloseToolbar() {
							m_Toolbar->SetVisibility(false);
							m_Toolbar->NotifyParentOnVisibilityChanged();
						}

					private:
						LayoutWidget* m_Toolbar = nullptr;
					};
					auto toolbarController = new ToolbarController();


					auto leftSplitColumn = Flexbox::Column().ID("leftSplitColumn").Parent(middleRow); {
						auto* toolbarBtn1 = Button::TextButton(new TooltipSpawner(leftSplitColumn, []() { return Tooltip::Text("Press me to toggle toolbar"); }), "Tool 1\nOpen sidebar");

						toolbarBtn1->SetCallback([=](bool bPressed) {
							if(!bPressed) return;
							static auto bVisible = true;
							bVisible = !bVisible;

							if(bVisible) toolbarController->OpenToolbar();
							else toolbarController->CloseToolbar();
						});

						auto* toolbarBtn2 = Button::TextButton(leftSplitColumn, "Tool 2\nIncrease size");

						toolbarBtn2->SetCallback([btn = toolbarBtn2](bool bPressed) {
							if(!bPressed) return;
							static int size = 0;
							size += 10;

							btn->SetSize(btn->GetChild<LayoutWidget>()->GetSize() + float2(10) * 2.f + float2(size, 0.f));
							btn->NotifyParentOnSizeChanged(AxisX);
						});
					}

					auto leftToolboxSplitBox = new SplitBox(middleRow, true, "Left_Tool_Main_Area_Split"); {

						auto leftToolboxColumn = Flexbox::Column().ID("Left_Toolbar_Column").ExpandCrossAxis(true).Parent(leftToolboxSplitBox); {
							Button::TextButton(leftToolboxColumn, "Tool Content");
						}
						toolbarController->SetToolbarWindow(leftToolboxColumn);

						auto rightMainAreaColumn = Flexbox::Column().ID("Right_Main_Area_Column").ExpandCrossAxis(true).Parent(leftToolboxSplitBox); {
							Button::TextButton(rightMainAreaColumn, "Main Area Content");
						}
					}
				}

				auto bottomRow = Flexbox::Row().ID("Status_Bar_Row").Parent(column); {
					auto* b = Button::TextButton(bottomRow, "Status bar");
				}
			}
		}
	}
}

int main(int argc, char* argv[]) {

	const auto commandLine = std::vector<std::string>(argv, argv + argc);

	using namespace UI;

	g_Application = Application::Create("App", 1800, 900);

	CreateTheme();
	//BuildFlexboxOverflowTestLayout();
	BuildAppScaffold();

	while(g_Application->Tick());
}