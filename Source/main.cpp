#include <iostream>

#include "Runtime/Core/BaseTypes.h"
#include "Runtime/System/JobDispatcher.h"
#include "Editor/UI/UI.h"

void SearchingJob(JobSystem::JobContext&);
void JobSystemTest(JobSystem::JobContext&);

void FiberTest() {
	//SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);	
	OPTICK_START_CAPTURE(Optick::Mode::INSTRUMENTATION);

	JobSystem::Init();

	JobSystem::Builder builder;
	builder.PushBack("Job 5", SearchingJob);
	builder.PushAsync("Test jobs", JobSystemTest);

	JobSystem::Submit(builder);

	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	//JobSystem::Shutdown();

	OPTICK_STOP_CAPTURE();
	OPTICK_SAVE_CAPTURE("F:\\Code\\C++\\Fibers_Test\\binary\\Capture.opt");
}

void JobSystemTest(JobSystem::JobContext&) {

	for(auto i = 0; i != 10; ++i) {
		//OPTICK_FRAME("Main");
		OPTICK_EVENT("Prepare Jobs");
		// Use case 1: Append jobs on the fly to the current job
		enum class Status {
			Error, Ok
		};

		struct Data {
			std::vector<int> data;
			int sum;
		};

		auto sharedData = new Data();
		sharedData->data.assign(100'000, 2);

		JobSystem::Builder builder;

		const auto job = [sharedData](JobSystem::JobContext&) {
			OPTICK_EVENT("First job");
			uint64_t accumulator = 0;
			for(const auto& data : sharedData->data) {
				accumulator += data;
			}
			//std::cout << "Sum is" << accumulator << std::endl;
			return Status::Ok;
		};

		const auto job2 = [sharedData](JobSystem::JobContext&) {
			OPTICK_EVENT("Second Job");
			uint64_t accumulator = 0;
			for(const auto& data : sharedData->data) {
				accumulator += data;
			}
			//std::cout << "Sum is" << accumulator << std::endl;
			return Status::Ok;
			};

		const auto job3 = [sharedData](JobSystem::JobContext&) {
			OPTICK_EVENT("Third Job");
			uint64_t accumulator = 0;
			for(const auto& data : sharedData->data) {
				accumulator += data;
			}
			//std::cout << "Sum is" << accumulator << std::endl;
			return Status::Ok;
			};

		const auto job4 = [sharedData](JobSystem::JobContext&) {
			OPTICK_EVENT("Forth Job");
			uint64_t accumulator = 0;
			for(const auto& data : sharedData->data) {
				accumulator += data;
			}
			//std::cout << "Sum is" << accumulator << std::endl;
			return Status::Ok;
			};

			// Implicit fence here
			//builder.Fence();
		builder.PushAsync("First job", job);
		builder.PushAsync("Second job", job2);
		builder.PushAsync("Third job", job3);
		builder.PushAsync("Forth job", job4);

		builder.PushBack("Job 5", job4);

		builder.PushBack("Job 6", job4);
		builder.PushAsync("Job 7", job4);
		builder.PushAsync("Job 8", job4);
		builder.PushAsync("Job 9", job4);

		// Explicit fence (sync point)
		//auto fence = builder.PushFence();

		//builder.KickAndWait();
		// OR
		builder.Kick();

		// Do some work ...

		// Wait untill fence is reached
		//fence.Wait();
	}
}

//void ReadFile() {
//
//	auto fbxFilePath = "C//Some path";
//	auto fileSize = FileSystem::GetFilesize(fbxFilePath);
//	auto* buffer = new Buffer();
//
//	auto reqHandle = IODispatcher::ReadAsync(fbxFilePath, fileSize, buffer);
//	JobSystem::Wait(reqHandle); // Current fiber gets descheduled
//
//	// DO some work ...
//	auto status = IODispatcher::CheckStatus(reqHandle);
//
//	if(status == IOStatus::Pending) {
//
//		// We can create a continuation job
//		JobSystem::SubmitSingle([buffer, reqHandle]() {
//			JobSystem::Wait(reqHandle);
//
//			if(reqHandle.GetError() != Error::Ok) {
//				PrintError(reqHandle.GetErrorMessage());
//			}
//		});
//		
//	} else if(status == IOStatus::Complete) {
//		// If completed here we can continue here
//		auto* fbxDoc = new FBXDoc{};
//		auto result = fbxDoc->ParseBuffer(buffer);
//
//		if(result == Result::HasErrors) {
//			fbxDoc.GetErrors();
//		}
//
//		// Traverse nodes
//
//	} else /*status == IOStatus::Error*/ {
//		// Error
//		PrintError("Cannot read file {}: {}", fbxFilePath, status.GetMessage());
//	}
//	
//}

void SearchingJob(JobSystem::JobContext&) {

	std::vector<int> data;
	constexpr auto splitNum = 8;
	constexpr auto needle = 2;

	std::vector<int> numberOfTwosFoundPerThread;
	JobSystem::Builder builder;

	for(auto i = 0; i != 10'000; ++i) {
		data.push_back(Math::Random::Integer<int>(0, 10));
	}
	numberOfTwosFoundPerThread.assign(splitNum, 0);

	// Parallel search
	for(auto i = 0; i != splitNum; ++i) {
		builder.PushAsync(
			std::format("SplitJob {}", i),
			[blockIndex = i, &data, &numberOfTwosFoundPerThread](JobSystem::JobContext&) {
				OPTICK_EVENT("Parallel search block");
				const auto blockSize = data.size() / splitNum;
				const auto start = blockIndex * blockSize;
				const auto end = start + blockSize;

				for(auto i = start; i != end; ++i) {
					const auto& val = data[i];

					if(val == needle) {
						numberOfTwosFoundPerThread[blockIndex]++;
					}
				}
			}
		);
	}
	auto completedEvent = builder.PushFence();

	JobSystem::Submit(builder);
	JobSystem::ThisFiber::WaitForEvent(completedEvent);

	std::cerr << "Number of twos found " << numberOfTwosFoundPerThread[0];
}

/*buttonToggleExpand->SetCallback(
						[button = buttonToggleExpand](bool bPressed) {
							if(!bPressed) return;

							static bool mode = false;
							mode = !mode;

							if(mode) {
								button->SetAxisMode(AxisX, AxisSize::Max);

								if(auto* parent = button->GetParent()) {
									ChildEvent event(button, Axis::Both, ChildEvent::OnAxisMode);
									parent->OnEvent(&event);
								}

							} else {
								button->SetAxisMode(AxisX, AxisSize::Min);
								button->SetSize(button->GetChild()->GetSize() + float2(10) * 2.f);

								if(auto* parent = button->GetParent()) {
									ChildEvent event(button, Axis::Both, ChildEvent::OnAxisMode);
									parent->OnEvent(&event);
								}
							}
						}
					);*/



UI::Application* g_Application = nullptr;

void CreateTheme() {

	auto theme = new UI::Theme();
	
	// Layout parameters for all widgets
	theme->Layout("").Margins(10, 10).Paddings(10, 10);
	// Layout parameters for class "Button"
	theme->Layout("Button").Margins(5, 5).Paddings(10, 10);
	
	// Style parameters for class:state:shape
	theme->Box("Button:Normal").FillColor("#454545").Borders(1.f).BorderColor("#343434");
	theme->Box("Button:Hovered", "Button:Normal").FillColor("#808080");
	theme->Box("Button:Pressed", "Button:Hovered").FillColor("#eeeeee");

	theme->Text("Text").Color("#ffffff").Size(20);
	
	g_Application->SetTheme(theme);
}

void BuildSimpleLayout() {
	using namespace UI;
	auto* root = g_Application->GetRoot();

	auto middleRow = Flexbox::Row(root, "Main_Middle_Row", true);

	auto column1 = Flexbox::Column(middleRow, "Column 1"); {
		Button::TextButton(column1, "Tool 1\nOpen sidebar");
		Button::TextButton(column1, "Tool 2\nIncrease size");
	}

	auto column2 = Flexbox::Column(middleRow, "Column 2"); {
		Button::TextButton(column2, "Tool 1\nOpen sidebar");
		Button::TextButton(column2, "Tool 2\nIncrease size");
	}
}

void BuildFlexboxOverflowTestLayout() {
	using namespace UI;
	auto* root = g_Application->GetRoot();

	
	auto splitBox = new SplitBox(root, true, "Splitbox"); {

		auto row = new Flexbox(splitBox, FlexboxDesc{
			.ID = "Main Row", 
			.Direction = ContentDirection::Row, 
			.JustifyContent = JustifyContent::Start,
			.bExpandMainAxis = true}); 
		{

			auto leftRow = new Flexbox(row, FlexboxDesc{
				.ID = "Left Row", 
				.Direction = ContentDirection::Row,
				.JustifyContent = JustifyContent::Start,
				.OverflowPolicy = OverflowPolicy::ShrinkWrap,
				.bExpandMainAxis = true}); 
			{
				Button::TextButton(leftRow, "Button 1");
				Button::TextButton(leftRow, "Button 2");
			}

			auto rightRow = new Flexbox(row, FlexboxDesc{
				.ID = "Right Row", 
				.Direction = ContentDirection::Row, 
				.JustifyContent = JustifyContent::End,
				.OverflowPolicy = OverflowPolicy::ShrinkWrap, 
				.bExpandMainAxis = true}); 
			{
				Button::TextButton(rightRow, "Button 1");
				Button::TextButton(rightRow, "Button 2");
			}
		}
		Button::TextButton(splitBox, "Placeholder");
	}
	
}

void BuildAverageApp() {
	using namespace UI;

	auto* root = g_Application->GetRoot(); {

		auto* centered = new Centered(root, "Root_Centered"); {

			auto* column = Flexbox::Column(centered, "Main_Column", true); {

				auto menuBar = Flexbox::Row(column, "Menu_Bar"); {

					auto leftGroup = new Flexbox(menuBar, FlexboxDesc{.ID = "Left_Group", .Direction = ContentDirection::Row}); {
						Button::TextButton(leftGroup, "Menu Item 1");
						Button::TextButton(leftGroup, "Menu Item 2");
					}

					auto rightGroup = new Flexbox(menuBar,
												  FlexboxDesc{.ID = "Right_Group", .Direction = ContentDirection::Row, .JustifyContent = JustifyContent::End}); {
						Button::TextButton(rightGroup, "Menu Item 3");
						Button::TextButton(rightGroup, "Menu Item 4");
					}
				}

				auto middleRow = Flexbox::Row(column, "Main_Middle_Row", true); {

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


					auto leftSplitColumn = Flexbox::Column(middleRow, "leftSplitColumn"); {
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

						auto leftToolboxColumn = Flexbox::Column(leftToolboxSplitBox, "Left_Toolbar_Column", true); {
							Button::TextButton(leftToolboxColumn, "Tool Content");
						}
						toolbarController->SetToolbarWindow(leftToolboxColumn);

						auto rightMainAreaColumn = Flexbox::Column(leftToolboxSplitBox, "Right_Main_Area_Column", true); {
							Button::TextButton(rightMainAreaColumn, "Main Area Content");
						}
					}
				}

				auto bottomRow = Flexbox::Row(column, "Status_Bar_Row"); {
					auto* b = Button::TextButton(bottomRow, "Status bar");
				}
			}
		}
	}
}



int main(int argc, char* argv[]) {

	const auto commandLine = std::vector<std::string>(argv, argv + argc);

	using namespace UI;

	g_Application = Application::Create("App", 1600, 1200);

	CreateTheme();
	BuildFlexboxOverflowTestLayout();
	//BuildAverageApp();
	
	while(g_Application->Tick());
}