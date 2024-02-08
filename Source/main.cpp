#include <iostream>

#include "Runtime/Core/BaseTypes.h"
#include "Runtime/System/JobDispatcher.h"
#include "Editor/UI/UI.h"

UI::Application* g_Application = nullptr;

void CreateTheme() {
	using namespace UI;
	auto* theme = new UI::Theme();

	// Button
	{
		auto& buttonStyle = theme->Add("Button"); {
			buttonStyle.Add<LayoutStyle>("Normal").SetMargins(5, 5).SetPaddings(5, 5);
			buttonStyle.Add<LayoutStyle>("Hovered", "Normal");
			buttonStyle.Add<LayoutStyle>("Pressed", "Normal");

			buttonStyle.Add<BoxStyle>("Normal").SetFillColor("#454545").SetRounding(6);
			buttonStyle.Add<BoxStyle>("Hovered", "Normal").SetFillColor("#808080");
			buttonStyle.Add<BoxStyle>("Pressed", "Normal").SetFillColor("#eeeeee");
		}

		auto& Tooltip = theme->Add("Tooltip"); {
			Tooltip.Add<LayoutStyle>().SetMargins(5).SetPaddings(5);
			Tooltip.Add<BoxStyle>().SetFillColor("#dd5050").SetRounding(6);
		}
	}
	Application::Get()->SetTheme(theme);
}

void BuildAppScaffold() {
	using namespace UI;

	auto& root = g_Application->GetRoot(); {

		auto* centered = new Centered(root, "Root_Centered"); {

			auto* column = FlexboxBuilder()
				.ID("Main_Column")
				.Direction(ContentDirection::Column)
				.OverflowPolicy(OverflowPolicy::Clip)
				.JustifyContent(JustifyContent::Center)
				.ExpandMainAxis(true)
				.ExpandCrossAxis(true)
				.Attach(centered->ChildSlot);
			{
				auto* menuBar = Flexbox::Row().ID("Menu Bar").Attach(column->ChildrenSlot); {

					auto* leftGroup = Flexbox::Row()
						.ID("Left_Group")
						.Attach(menuBar->ChildrenSlot);
					{
						Button::TextButton(leftGroup->ChildrenSlot, "Menu Item 1");
						Button::TextButton(leftGroup->ChildrenSlot, "Menu Item 2");
					}

					auto* rightGroup = Flexbox::Row()
						.ID("Right_Group")
						.JustifyContent(UI::JustifyContent::End)
						.Attach(menuBar->ChildrenSlot); 
					{
						Button::TextButton(rightGroup->ChildrenSlot, "Menu Item 3");
						Button::TextButton(rightGroup->ChildrenSlot, "Menu Item 4");
					}
				}

				auto* middleRow = Flexbox::Row()
					.ID("Main_Middle_Row")
					.ExpandCrossAxis(true)
					.Attach(column->ChildrenSlot); 
				{
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
					auto* toolbarController = new ToolbarController();

					auto* leftSplitColumn = Flexbox::Column().ID("leftSplitColumn").Attach(middleRow->ChildrenSlot); {

						//auto* tooltip = new TooltipSpawner(leftSplitColumn->ChildrenSlot, []() { return Tooltip::Text("Press me to toggle toolbar"); });
						//auto* toolbarBtn1 = Button::TextButton(tooltip->ChildSlot, "Tool 1\nOpen sidebar");

						auto* toolbarBtn11 = ButtonBuilder()
							.ID("Btn 1")
							.Text("Tool 1\nOpen sidebar")
							.Tooltip(TooltipSpawner::Text("Opens a toolbar"))
							.Callback([=](bool bPressed) {
								if(!bPressed) return;
								static auto bVisible = true;
								bVisible = !bVisible;

								if(bVisible) toolbarController->OpenToolbar();
								else toolbarController->CloseToolbar();
							})							
							.Attach(leftSplitColumn->ChildrenSlot);													
							
						auto* toolbarBtn2 = Button::TextButton(leftSplitColumn->ChildrenSlot, "Tool 2\nIncrease size");

						toolbarBtn2->SetCallback([btn = toolbarBtn2](bool bPressed) {
							if(!bPressed) return;
							static int size = 0;
							size += 10;

							btn->SetSize(btn->GetChild<LayoutWidget>()->GetSize() + float2(10) * 2.f + float2(size, 0.f));
							btn->NotifyParentOnSizeChanged(AxisX);
						});
					}

					auto* leftToolboxSplitBox = new SplitBox(middleRow->ChildrenSlot, true, "Left_Tool_Main_Area_Split"); {

						auto* leftToolboxColumn = Flexbox::Column().ID("Left_Toolbar_Column").ExpandCrossAxis(true).Attach(leftToolboxSplitBox->FirstSlot); {
							Button::TextButton(leftToolboxColumn->ChildrenSlot, "Tool Content");
						}
						toolbarController->SetToolbarWindow(leftToolboxColumn);

						auto* rightMainAreaColumn = Flexbox::Column().ID("Right_Main_Area_Column").ExpandCrossAxis(true).Attach(leftToolboxSplitBox->SecondSlot); {
							Button::TextButton(rightMainAreaColumn->ChildrenSlot, "Main Area Content");
						}
					}
				}

				auto* bottomRow = Flexbox::Row().ID("Status_Bar_Row").Attach(column->ChildrenSlot); {
					auto* b = Button::TextButton(bottomRow->ChildrenSlot, "Status bar");
				}
			}
		}
	}
}














//class BreadCrumbsList: public UI::Controller {
//public:
//
//	BreadCrumbsList(FilesystemRootController* inRootController) {
//
//
//	}
//
//
//};
//
//class FileTiles: public UI::Controller {
//public:
//
//	FileTiles(FilesystemRootController* inRootController) {
//
//
//	}
//
//	void AddItem(FileInfo inFIle) {
//		auto itemView = new ItemView(inFIle);
//		Container->ChildrenSlot.Attach(itemView);
//	}
//
//	void OnItemSelected(ItemView* inItem) {
//
//	}
//
//	void OnItemClicked(ItemVIew* inItem) {
//		filelist = Filesystem::GetFilesInDirectory(inFile.IsDirectory());
//		fileTiles.Clear();
//
//		auto chidlrenList;
//		Build();		
//
//		RootController->OnItemOpened(Path);
//	}
//
//	void Build(childrenlist) {
//		{
//			for(auto file : filelist) {
//				fileTiles.AddItem(file);
//			}
//		}
//	}
//
//	FilesystemRootController* RootController;
//	Flexbox* Container;
//};
//
//class FilesystemRootController: public UI::Controller {
//public:
//
//	FilesystemRootController(const Path& inRootPath) {
//
//	}
//
//	Widget* Build() {
//		auto fileTiles = new FileTiles(this);
//		auto breadCrumbs = new BreadCrumbsList(this);
//		auto forwardBtn = new Button(setCallback());
//		auto backBtn = new Button(setCallback());
//		auto upBtn = new Button();
//
//
//		auto flexbox = new Flexbox();
//		flexbox->ChildrenSlot.Attach({fileTiles, breadCrumbs});
//	}
//
//	void OnFileClicked(Path inFile) {
//		breadCrumbs.SetPath(inFile);
//		fileTiles.OpenDirectory(inFile);
//	}
//
//	Path m_Rootpath;
//};





int main(int argc, char* argv[]) {

	const auto commandLine = std::vector<std::string>(argv, argv + argc);

	using namespace UI;

	g_Application = Application::Create("App", 1800, 900);

	CreateTheme();
	BuildAppScaffold();

	while(g_Application->Tick());
}