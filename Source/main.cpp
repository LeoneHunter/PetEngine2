#include <iostream>
#include <Assert.h>
#include <chrono>
#include <thread>
#include <vector>
#include <format>
#include <regex>
#include <functional>
#include <deque>
#include <mutex>
#include <random>
#include <set>
#include <semaphore>

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


int main(int argc, char* argv[]) {

	using namespace UI;

	//TODO Save command line
	// save working directory
	Init("App", 800, 600);

	auto* root = GetRoot();{

		auto* centered = new Centered(root, "Root_Centered");{

			auto* column = Flexbox::Column(centered, "Main_Column", true);{

				auto menuBar = Flexbox::Row(column, "Menu_Bar");{

					auto leftGroup = new Flexbox(menuBar, FlexboxDesc{.ID = "Left_Group", .Direction = ContentDirection::Row});{
						Button::TextButton(leftGroup, "Menu Item 1");
						Button::TextButton(leftGroup, "Menu Item 2");
					}

					auto rightGroup = new Flexbox(menuBar, 
												  FlexboxDesc{.ID = "Right_Group", .Direction = ContentDirection::Row, .JustifyContent = JustifyContent::End});{
						Button::TextButton(rightGroup, "Menu Item 3");
						Button::TextButton(rightGroup, "Menu Item 4");
					}					
				}

				auto exp = new Expanded(column);
				auto middleRow = Flexbox::Row(exp, "Main_Middle_Row"); {

					auto leftVerticalToolbar = Flexbox::Column(middleRow, "Left_Vertical_Toolbar_Column");{					
						auto* toolbarBtn1 = Button::TextButton(leftVerticalToolbar, "Tool 1");
						auto* toolbarBtn2 = Button::TextButton(leftVerticalToolbar, "Tool 2");

						toolbarBtn2->SetCallback([btn = toolbarBtn2](bool bPressed) {
							if(!bPressed) return;
							static int size = 0;
							size += 10;

							btn->SetSize(btn->GetChild()->GetSize() + float2(10) * 2.f + float2(size, 0.f));
							btn->NotifyParentOnSizeChanged(AxisX);
						});
					}

					auto mainSplitBox = new SplitBox(middleRow, true, "Main_Split");{
						Button::TextButton(mainSplitBox, "Left");
						Button::TextButton(mainSplitBox, "Right");
					}
				}

				auto bottomRow = Flexbox::Row(column, "Status_Bar_Row");{				
					auto* b = Button::TextButton(bottomRow, "Status bar");
				}
			}
		}
	}	
	
	while(Tick());
}