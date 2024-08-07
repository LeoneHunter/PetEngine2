#include <iostream>
#include <ranges>

#include "editor/ui/ui.h"
#include "runtime/win_minimal.h"
#include "runtime/threading.h"
#include "runtime/rtti.h"

namespace test_focus {

class Window: public WidgetState {
	WIDGET_CLASS(Window, WidgetState)
public:

	Window(std::string_view title)
		: title_(title)
		, focusNode_([this](auto v) { OnFocusChanged(v); })
	{}

	std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) {
		return Focused::New(
			&focusNode_,
			MouseRegion::Build()
				.OnMouseButton([this](const MouseButtonEvent& e) {
					if(e.button == MouseButton::ButtonLeft && e.isPressed) {
						focusNode_.RequestFocus();
					}
				})
				.Child(Container::Build()
					.StyleClass("Window")
					.SizeFixed(400, 400)	
					.Child(Flexbox::Build()
						.DirectionColumn()
						.Expand()
						.Children(Container::Build()
							.StyleClass("TitleBar")
							.BoxStyle(isFocused_ ? "Focused" : "")
							.SizeMode(AxisMode::Expand, AxisMode::Fixed)
							.Size(0, 20)
							.Child(Aligned::New(
								Alignment::Center,
								Alignment::Center,
								TextBox::New(title_)
							))
							.New()
						)
						.New()
					)
					.New()
				)
				.New()
		);
	}

private:

	void OnFocusChanged(bool focused) {
		isFocused_ = focused;
		LOGF(Verbose, "{} is {}", GetDebugID(), focused ? "focused" : "unfocused");
		MarkNeedsRebuild();
	}

private:
	bool		isFocused_ = false;
	std::string title_;
	FocusNode   focusNode_;
};

class App: public WidgetState {
public:

	App() {
		for(auto i = 0; i < 2; ++i) {
			windows_.emplace_back(new Window(std::format("Window {}", i)));
		}
	}

	std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) {
		return Flexbox::Build()
			.DirectionRow()
			.JustifyContent(JustifyContent::SpaceAround)
			.AlignCenter()
			.Expand()
			.Children([this]() {
				std::vector<std::unique_ptr<Widget>> out;
				for(auto& w: windows_) {
					out.emplace_back(StatefulWidget::New(w.get()));
				}
				return out;
			}())
			.New();
	}

private:
	std::vector<std::unique_ptr<WidgetState>> windows_;
};
App* app;

void BuildTestWidgets() {
	app = new App();
	g_Application->Parent(StatefulWidget::New(app));
}

} // namespace test_focus

void RunUI() {
	g_Application = Application::Create("App", 1800, 900);
	SetDarkTheme(g_Application->GetTheme());

	auto app = std::make_unique<FilesystemView>(std::path("G:\\"));
	g_Application->Parent(StatefulWidget::New(app.get()));
	// test_focus::BuildTestWidgets();

	g_Application->Run();
}


int main(int argc, char* argv[]) {
	CommandLine::Set(argc, argv);
	windows::SetConsoleCodepageUtf8();
	Thread::SetCurrentThreadName("Main Thread");

	logging::Init(CommandLine::GetWorkingDir().string());
	logging::SetLevel(logging::Level::All);

	RunUI();
}