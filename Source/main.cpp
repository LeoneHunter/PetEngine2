#include <iostream>

#include "Runtime/Core/BaseTypes.h"
#include "Runtime/System/JobDispatcher.h"
#include "Editor/UI/UI.h"

using namespace UI;
UI::Application* g_Application = nullptr;

void SetDarkTheme() {
	auto* theme = new UI::Theme();
	{
		auto& buttonStyle = theme->Add("Button");
		buttonStyle.Add<LayoutStyle>("Normal").SetMargins(5).SetPaddings(5);
		buttonStyle.Add<LayoutStyle>("Hovered", "Normal");
		buttonStyle.Add<LayoutStyle>("Pressed", "Normal");

		buttonStyle.Add<BoxStyle>("Normal").SetFillColor("#505050").SetBorders(0).SetRounding(6);
		buttonStyle.Add<BoxStyle>("Hovered", "Normal").SetFillColor("#707070");
		buttonStyle.Add<BoxStyle>("Pressed", "Normal").SetFillColor("#909090");
	}
	{
		auto& windowStyle = theme->Add("FloatWindow");
		windowStyle.Add<LayoutStyle>().SetMargins(5, 5).SetPaddings(5, 5);
		windowStyle.Add<BoxStyle>().SetFillColor("#303030").SetRounding(6);		
	}
	{
		auto& windowStyle = theme->Add("Popup");
		windowStyle.Add<LayoutStyle>().SetMargins(5, 5).SetPaddings(5, 5);
		windowStyle.Add<BoxStyle>().SetFillColor("#202020").SetRounding(6).SetBorders(1).SetBorderColor("#202020");	

		theme->Add("ContextMenu", "Popup");
	}		
	{
		auto& tooltip = theme->Add("Tooltip");
		tooltip.Add<LayoutStyle>().SetMargins(5).SetPaddings(5);
		tooltip.Add<BoxStyle>().SetFillColor("#303030").SetRounding(4).SetBorders(1).SetBorderColor("#202020");
	}
	g_Application->SetTheme(theme);
}



std::unique_ptr<PopupWindow> Popup1(PopupSpawnContext inCtx) {

	auto popup = PopupBuilder()
		.ID("MyPopup")
		.StyleClass("MyPopup")
		.Position(inCtx.MousePosGlobal)
		.Size(200, 400)
		.Create();
	{
		auto* column = FlexboxBuilder()
			.ID("Main_Column")
			.DirectionColumn()
			.Expand()
			.JustifyContent(JustifyContent::Center)
			.Create(popup.get());
		{
			ButtonBuilder()
				.Text("Button 1")
				.Wrap<PopupSpawner>(Popup1)
				.Create(column);

			ButtonBuilder()
				.Text("Menu Item 1")
				.Wrap<PopupMenuItem>()
				.Wrap<TooltipSpawner>(TextTooltip("I'm a tooltip 1"))
				.Create(column);

			ButtonBuilder()
				.Text("Menu Item 2")
				.Wrap<PopupMenuItem>()
				.Wrap<TooltipSpawner>(TextTooltip("I'm a tooltip 2"))
				.Create(column);

			ButtonBuilder()
				.Text("Menu Item 3")
				.Wrap<PopupMenuItem>()
				.Wrap<TooltipSpawner>(TextTooltip("I'm a tooltip 3"))
				.Create(column);
		}
	}

	auto windowPos = inCtx.MousePosGlobal;
	auto* layoutChild = inCtx.Spawner->GetChild<LayoutWidget>();

	if(layoutChild) {
		windowPos = layoutChild->GetRectGlobal().BL();
	}
	const auto popupRect = popup->GetRect();

	for(auto axis = 0; axis < 2; ++axis) {
		if(popupRect.max[axis] > inCtx.ViewportSize[axis]) {
			windowPos[axis] = inCtx.ViewportSize[axis] - popupRect.Size()[axis];
		}
	}
	popup->SetOrigin(windowPos);
	return popup;
}

void BuildSimple() {

	// Popup 2
	auto popup2 = [](PopupSpawnContext inCtx) {

		auto popup = PopupBuilder()
			.StyleClass("Popup")
			.Position(inCtx.MousePosGlobal)
			.Size(200, 400)
			.Create();
		{
			auto* column = FlexboxBuilder()
				.DirectionColumn()
				.Expand()
				.JustifyContent(JustifyContent::Center)
				.Create(popup.get());
			{
				ButtonBuilder()
					.Text("Menu Item 1")
					.Wrap<PopupMenuItem>()
					.Create(column);

				ButtonBuilder()
					.Text("Menu Item 2")
					.Wrap<PopupMenuItem>()
					.Create(column);

				ButtonBuilder()
					.Text("Menu Item 3")
					.Wrap<PopupMenuItem>()
					.Create(column);
			}
		}	
		return popup;
	};


	auto contextMenu = [](PopupSpawnContext inCtx) {
		auto menu = ContextMenuBuilder{}
			.Position(inCtx.MousePosGlobal)
			.MenuItem("Item 1")
			.MenuItem("Item 2")
			.MenuItem("Item 3")
			.SubMenu("Submenu 1", [](PopupSpawnContext) {
				return ContextMenuBuilder()
					.MenuItem("Sub menu item 1")
					.MenuItem("Sub menu item 2")
					.MenuItem("Sub menu item 3")
					.Create();
			})
			.SubMenu("Submenu 2", [](PopupSpawnContext) {
				return ContextMenuBuilder()
					.MenuItem("Sub menu item 4")
					.MenuItem("Sub menu item 5")
					.MenuItem("Sub menu item 6")
					.SubMenu("Submenu 1", [](PopupSpawnContext) {
						return ContextMenuBuilder()
							.MenuItem("Sub menu item 7")
							.MenuItem("Sub menu item 8")
							.MenuItem("Sub menu item 9")
							.Create();
					})
					.Create();
			})
			.Create();
		return menu;
	};


	// Main floating window
	auto* window1 = WindowBuilder()
		.ID("Window1")
		.StyleClass("FloatWindow")
		.Position(200, 200)
		.Size(600, 400)
		.Create(g_Application);
	{
		auto* column = FlexboxBuilder()
			.ID("Column")
			.DirectionColumn()
			.Expand()
			.JustifyContent(JustifyContent::Center)
			.Wrap<PopupSpawner>(contextMenu)
			.Create(window1);
		{
			ButtonBuilder()
				.Text("Popup 1")
				.Wrap<PopupSpawner>(popup2)
				.Wrap<TooltipSpawner>(TextTooltip("Opens a popup 2 menu"))
				.Create(column);

			ButtonBuilder()
				.Text("Button 2")
				.Wrap<TooltipSpawner>(TextTooltip("I'm a Button 2"))
				.Create(column);

			ButtonBuilder()
				.Text("Button 3")
				.Wrap<TooltipSpawner>(TextTooltip("I'm a Button 3"))
				.Create(column);
		}
	}
}

void BuildAppScaffold() {
	//using namespace UI;
	//{
	//	auto& style = g_Application->GetTheme()->Add("Window");
	//	style.Add<BoxStyle>().SetFillColor("#101030").SetRounding(6);
	//	style.Add<LayoutStyle>().SetMargins(5, 5).SetPaddings(5, 5);
	//}
	//{
	//	auto& style = g_Application->GetTheme()->Add("BackgroundWindow");
	//	style.Add<BoxStyle>().SetFillColor("#101030").SetRounding(0);
	//	style.Add<LayoutStyle>().SetMargins(0).SetPaddings(0);
	//}
	//g_Application->GetTheme()->Add("MyPopup", "Window");

	//	
	//// Background window
	//auto* backgroundWindow = new Window(g_Application, "BackgroundWindow", WindowFlags::Background, "BackgroundWindow");
	//auto* centered = new Centered(backgroundWindow->DefaultSlot, "Root_Centered"); {

	//	auto* column = FlexboxBuilder()
	//		.ID("Main_Column")
	//		.Direction(ContentDirection::Column)
	//		.OverflowPolicy(OverflowPolicy::Clip)
	//		.JustifyContent(JustifyContent::Center)
	//		.ExpandMainAxis(true)
	//		.ExpandCrossAxis(true)
	//		.Create(centered->DefaultSlot);
	//	{
	//		auto* menuBar = Flexbox::Row().ID("Menu Bar").Create(column->DefaultSlot); {

	//			auto* leftGroup = Flexbox::Row()
	//				.ID("Left_Group")
	//				.Create(menuBar->DefaultSlot);
	//			{
	//				ButtonBuilder().Text("Menu item 1")
	//					.Wrap<PopupSpawner>(Popup1)
	//					.Create(leftGroup->DefaultSlot);

	//				Button::TextButton(leftGroup->DefaultSlot, "Menu Item 2");
	//			}

	//			auto* rightGroup = Flexbox::Row()
	//				.ID("Right_Group")
	//				.JustifyContent(UI::JustifyContent::End)
	//				.Create(menuBar->DefaultSlot); 
	//			{
	//				Button::TextButton(rightGroup->DefaultSlot, "Menu Item 3");
	//				Button::TextButton(rightGroup->DefaultSlot, "Menu Item 4");
	//			}
	//		}

	//		auto* middleRow = Flexbox::Row()
	//			.ID("Main_Middle_Row")
	//			.ExpandCrossAxis(true)
	//			.Create(column->DefaultSlot); 
	//		{
	//			struct ToolbarController {

	//				void SetToolbarWindow(LayoutWidget* inWindow) {
	//					m_Toolbar = inWindow;
	//				}

	//				void OpenToolbar() {
	//					m_Toolbar->SetVisibility(true);
	//					m_Toolbar->NotifyParentOnVisibilityChanged();
	//				}

	//				void CloseToolbar() {
	//					m_Toolbar->SetVisibility(false);
	//					m_Toolbar->NotifyParentOnVisibilityChanged();
	//				}

	//			private:
	//				LayoutWidget* m_Toolbar = nullptr;
	//			};
	//			auto* toolbarController = new ToolbarController();

	//			auto* leftSplitColumn = Flexbox::Column().ID("leftSplitColumn").Create(middleRow->DefaultSlot); {

	//				//auto* tooltip = new TooltipSpawner(leftSplitColumn->ChildrenSlot, []() { return Tooltip::Text("Press me to toggle toolbar"); });
	//				//auto* toolbarBtn1 = Button::TextButton(tooltip->ChildSlot, "Tool 1\nOpen sidebar");

	//				auto* toolbarBtn11 = ButtonBuilder()
	//					.ID("Btn 1")
	//					.Text("Tool 1\nOpen sidebar")
	//					.Wrap<TooltipSpawner>(TextTooltip("Opens a toolbar"))
	//					.Wrap<PopupSpawner>(Popup1)
	//					.Callback([=](bool bPressed) {
	//						if(!bPressed) return;
	//						static auto bVisible = true;
	//						bVisible = !bVisible;

	//						if(bVisible) toolbarController->OpenToolbar();
	//						else toolbarController->CloseToolbar();
	//					})							
	//					.Create(leftSplitColumn->DefaultSlot);							
	//						
	//				auto* toolbarBtn2 = Button::TextButton(leftSplitColumn->DefaultSlot, "Tool 2\nIncrease size");

	//				toolbarBtn2->SetCallback([btn = toolbarBtn2](bool bPressed) {
	//					if(!bPressed) return;
	//					static int size = 0;
	//					size += 10;

	//					btn->SetSize(btn->GetChild<LayoutWidget>()->GetSize() + float2(10) * 2.f + float2((float)size, 0.f));
	//					btn->NotifyParentOnSizeChanged(AxisX);
	//				});
	//			}

	//			auto* leftToolboxSplitBox = new SplitBox(middleRow->DefaultSlot, true, "Left_Tool_Main_Area_Split"); {

	//				auto* leftToolboxColumn = Flexbox::Column().ID("Left_Toolbar_Column").ExpandCrossAxis(true).Create(leftToolboxSplitBox->FirstSlot); {
	//					Button::TextButton(leftToolboxColumn->DefaultSlot, "Tool Content");
	//				}
	//				toolbarController->SetToolbarWindow(leftToolboxColumn);

	//				auto* rightMainAreaColumn = Flexbox::Column().ID("Right_Main_Area_Column").ExpandCrossAxis(true).Create(leftToolboxSplitBox->SecondSlot); {
	//					Button::TextButton(rightMainAreaColumn->DefaultSlot, "Main Area Content");
	//				}
	//			}
	//		}

	//		auto* bottomRow = Flexbox::Row().ID("Status_Bar_Row").Create(column->DefaultSlot); {
	//			ButtonBuilder()
	//				.Text("Status bar")
	//				.Wrap<TooltipSpawner>(TextTooltip("This is the place for displaying some status app info"))
	//				.Wrap<PopupSpawner>(Popup1)
	//				.Create(bottomRow->DefaultSlot);
	//		}
	//	}
	//}
	
}

int main(int argc, char* argv[]) {

	const auto commandLine = std::vector<std::string>(argv, argv + argc);

	using namespace UI;

	g_Application = Application::Create("App", 1800, 900);

	SetDarkTheme();
	BuildSimple();
	//BuildAppScaffold();

	while(g_Application->Tick());
}