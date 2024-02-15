#include <iostream>

#include "Runtime/Core/BaseTypes.h"
#include "Runtime/System/JobDispatcher.h"
#include "Editor/UI/UI.h"

using namespace UI;
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

		auto& popupMenuItem = theme->Add("PopupMenuItem", "Button");

		auto& tooltip = theme->Add("Tooltip"); {
			tooltip.Add<LayoutStyle>().SetMargins(5).SetPaddings(5);
			tooltip.Add<BoxStyle>().SetFillColor("#dd5050").SetRounding(6);
		}
	}
	Application::Get()->SetTheme(theme);
}



void BuildAppScaffold() {
	using namespace UI;

	auto& windowStyle = g_Application->GetTheme()->Add("Window"); {
		windowStyle.Add<BoxStyle>().SetFillColor("#101030").SetRounding(6);
	}

	auto& popupStyle = g_Application->GetTheme()->Add("MyPopup"); {
		popupStyle.Add<BoxStyle>().SetFillColor("#101050").SetRounding(6);
	}

	auto* window1 = WindowBuilder()
		.ID("Window1")
		.StyleClass("Window")
		.Position(200, 200)
		.Size(600, 400)
		.Parent(g_Application);
	{
		auto* column = FlexboxBuilder()
			.ID("Main_Column")
			.DirectionColumn()
			.Expand()
			.JustifyContent(JustifyContent::Center)
			.Parent(window1->ChildSlot);
		{
			auto* popup = new PopupSpawner(column->ChildrenSlot, [&](Point inMousePosGlobal) {

				auto window1 = WindowBuilder()
					.ID("MyPopup")
					.StyleClass("MyPopup")
					.Position(inMousePosGlobal)
					.Size(100, 100)
					.Flags(UI::WindowFlags::Popup)
					.Create();
				{
					auto* column = FlexboxBuilder()
						.ID("Main_Column")
						.DirectionColumn()
						.Expand()
						.JustifyContent(JustifyContent::Center)
						.Parent(window1->ChildSlot);
					{
						//auto* popup = new MyMenuItem(column->ChildrenSlot);
						//auto* text = new Text(popup->ChildSlot, "Open a submenu");

						Button::TextButton(column->ChildrenSlot, "Menu Item 1");
						Button::TextButton(column->ChildrenSlot, "Menu Item 2");
						Button::TextButton(column->ChildrenSlot, "Menu Item 3");
					}
				}
				return window1;			
			});
			Button::TextButton(popup->ChildSlot, "Menu Item 1");

			Button::TextButton(column->ChildrenSlot, "Menu Item 2");
			Button::TextButton(column->ChildrenSlot, "Menu Item 3");
			Button::TextButton(column->ChildrenSlot, "Menu Item 4");

			auto* menuItem = new PopupMenuItem(column->ChildrenSlot, [&](Point inMousePosGlobal) {

				auto window1 = WindowBuilder()
					.ID("MyPopup")
					.StyleClass("MyPopup")
					.Position(inMousePosGlobal)
					.Size(100, 100)
					.Flags(UI::WindowFlags::Popup)
					.Create();
				{
					auto* column = FlexboxBuilder()
						.ID("Main_Column")
						.DirectionColumn()
						.Expand()
						.JustifyContent(JustifyContent::Center)
						.Parent(window1->ChildSlot);
					{
						//auto* popup = new MyMenuItem(column->ChildrenSlot);
						//auto* text = new Text(popup->ChildSlot, "Open a submenu");

						Button::TextButton(column->ChildrenSlot, "Menu Item 1");
						Button::TextButton(column->ChildrenSlot, "Menu Item 2");
						Button::TextButton(column->ChildrenSlot, "Menu Item 3");
					}
				}
				return window1;
			});
			auto* menuItemText = new Text(menuItem->ChildSlot, "Submenu");
		}
	}


	// Background window
	auto* backgroundWindow = new Window(g_Application, "BackgroundWindow", WindowFlags::Background);
	auto* centered = new Centered(backgroundWindow->ChildSlot, "Root_Centered"); {

		auto* column = FlexboxBuilder()
			.ID("Main_Column")
			.Direction(ContentDirection::Column)
			.OverflowPolicy(OverflowPolicy::Clip)
			.JustifyContent(JustifyContent::Center)
			.ExpandMainAxis(true)
			.ExpandCrossAxis(true)
			.Parent(centered->ChildSlot);
		{
			auto* menuBar = Flexbox::Row().ID("Menu Bar").Parent(column->ChildrenSlot); {

				auto* leftGroup = Flexbox::Row()
					.ID("Left_Group")
					.Parent(menuBar->ChildrenSlot);
				{
					Button::TextButton(leftGroup->ChildrenSlot, "Menu Item 1");
					Button::TextButton(leftGroup->ChildrenSlot, "Menu Item 2");
				}

				auto* rightGroup = Flexbox::Row()
					.ID("Right_Group")
					.JustifyContent(UI::JustifyContent::End)
					.Parent(menuBar->ChildrenSlot); 
				{
					Button::TextButton(rightGroup->ChildrenSlot, "Menu Item 3");
					Button::TextButton(rightGroup->ChildrenSlot, "Menu Item 4");
				}
			}

			auto* middleRow = Flexbox::Row()
				.ID("Main_Middle_Row")
				.ExpandCrossAxis(true)
				.Parent(column->ChildrenSlot); 
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

				auto* leftSplitColumn = Flexbox::Column().ID("leftSplitColumn").Parent(middleRow->ChildrenSlot); {

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
						.Parent(leftSplitColumn->ChildrenSlot);													
							
					auto* toolbarBtn2 = Button::TextButton(leftSplitColumn->ChildrenSlot, "Tool 2\nIncrease size");

					toolbarBtn2->SetCallback([btn = toolbarBtn2](bool bPressed) {
						if(!bPressed) return;
						static int size = 0;
						size += 10;

						btn->SetSize(btn->GetChild<LayoutWidget>()->GetSize() + float2(10) * 2.f + float2((float)size, 0.f));
						btn->NotifyParentOnSizeChanged(AxisX);
					});
				}

				auto* leftToolboxSplitBox = new SplitBox(middleRow->ChildrenSlot, true, "Left_Tool_Main_Area_Split"); {

					auto* leftToolboxColumn = Flexbox::Column().ID("Left_Toolbar_Column").ExpandCrossAxis(true).Parent(leftToolboxSplitBox->FirstSlot); {
						Button::TextButton(leftToolboxColumn->ChildrenSlot, "Tool Content");
					}
					toolbarController->SetToolbarWindow(leftToolboxColumn);

					auto* rightMainAreaColumn = Flexbox::Column().ID("Right_Main_Area_Column").ExpandCrossAxis(true).Parent(leftToolboxSplitBox->SecondSlot); {
						Button::TextButton(rightMainAreaColumn->ChildrenSlot, "Main Area Content");
					}
				}
			}

			auto* bottomRow = Flexbox::Row().ID("Status_Bar_Row").Parent(column->ChildrenSlot); {
				auto* b = Button::TextButton(bottomRow->ChildrenSlot, "Status bar");
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