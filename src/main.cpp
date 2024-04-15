#include <iostream>
#include <ranges>

#include "runtime/core/types.h"
#include "runtime/system/job_dispatcher.h"
#include "editor/ui/ui.h"

using namespace ui;
ui::Application* g_Application = nullptr;

void SetDarkTheme() {
	auto* theme = new ui::Theme();
	const auto frameColor = Color::FromHex("#202020");
	const auto hoveredColor = Color::FromHex("#505050");
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
		auto& s = theme->Add("ScrollbarContainer");
		s.Add<LayoutStyle>().Margins(0).Paddings(0);
		s.Add<BoxStyle>().Rounding(0).Opacity(0.f).Borders(0);
	}
	{
		auto& s = theme->Add("ScrollbarTrack");
		s.Add<LayoutStyle>("Normal").Margins(0).Paddings(0);
		s.Add<LayoutStyle>("Hovered", "Normal");
		s.Add<LayoutStyle>("Pressed", "Normal");

		s.Add<BoxStyle>("Normal").FillColor("#353535").Borders(0).Rounding(0);
		s.Add<BoxStyle>("Hovered", "Normal");
		s.Add<BoxStyle>("Pressed", "Normal");
	}
	{
		auto& s = theme->Add("ScrollbarThumb");
		s.Add<LayoutStyle>("Normal").Margins(0).Paddings(0);
		s.Add<LayoutStyle>("Hovered", "Normal");
		s.Add<LayoutStyle>("Pressed", "Normal");

		s.Add<BoxStyle>("Normal").FillColor("#505050").Borders(0).Rounding(3);
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
		s.Add<BoxStyle>().FillColor(frameColor).Rounding(4).Borders(1).BorderColor("#aaaaaa");	

		theme->Add("ContextMenu", "Popup");
	}		
	{
		auto& s = theme->Add("PopupMenuItem");
		s.Add<LayoutStyle>("Normal").Margins(0).Paddings(5);
		s.Add<LayoutStyle>("Hovered", "Normal");
		s.Add<LayoutStyle>("Pressed", "Normal");
		s.Add<LayoutStyle>("Opened", "Normal");

		s.Add<BoxStyle>("Normal").FillColor(frameColor).Borders(0).Rounding(4);
		s.Add<BoxStyle>("Hovered", "Normal").FillColor(hoveredColor);
		s.Add<BoxStyle>("Pressed", "Hovered");
		s.Add<BoxStyle>("Opened", "Hovered");
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
	g_Application->SetTheme(theme);
}

void TestFlexbox() {
	g_Application->Parent(
		Flexbox::Build()
			.DirectionRow()
			.ID("FlexboxTest")
			.Style("")
			.AlignCenter()
			.Children(
				Flexible::New(7, 
					Aligned::New(Alignment::Center, Alignment::Center, 
						Button::Build().Text("Button 1").SizeFixed({200, 20}).AlignContent(Alignment::Center, Alignment::Center).New()
					)
				),
				Flexible::New(3, 
					Aligned::New(Alignment::Center, Alignment::Center, 
						Button::Build().Text("Button 2").SizeFixed({300, 40}).AlignContent(Alignment::Center, Alignment::Center).New()
					)
				),
				Button::Build().Text("Button 3").New()
			)
			.New());
}

namespace std {
	using namespace std::filesystem;
}

// Draws a tree of directories in the specified folder
// Each tree node can be opened, dragged, deleted and created
class FilesystemView: public WidgetState {
	WIDGET_CLASS(FilesystemView, WidgetState)
public:

	FilesystemView(const std::path& dir) 
		: scrollViewState_(ScrollViewState::Flags::Vertical | ScrollViewState::Flags::ScrollbarVertical) {
		SetRootDirectory(dir);
		auto theme = Application::Get()->GetTheme();	
		const auto frameColor = Color::FromHex("#303030");
		const auto hoveredColor = Color::FromHex("#505050");
		
		auto& s = theme->Add("FilesystemNode");
		s.Add<LayoutStyle>("Normal").Margins(0).Paddings(5);
		s.Add<LayoutStyle>("Hovered", "Normal");
		s.Add<LayoutStyle>("Pressed", "Normal");

		s.Add<BoxStyle>("Normal").FillColor(frameColor).Borders(0).Rounding(3);
		s.Add<BoxStyle>("Hovered", "Normal").FillColor(hoveredColor);
		s.Add<BoxStyle>("Pressed", "Normal").FillColor(hoveredColor);
		{
			auto& s = theme->Add("Frame");
			s.Add<LayoutStyle>().Margins(5, 5).Paddings(5, 5);
			s.Add<BoxStyle>().FillColor(frameColor).Rounding(6);		
		}
	}

	std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) override {
		std::vector<std::unique_ptr<Widget>> out;
		std::function<void(Node&)> build;

		build = [&](Node& node) {
			out.push_back(StatefulWidget::New(&node));
			if(node.isOpen) {
				for(auto index: node.childrenIndices) {
					auto& child = nodes[index];
					build(*child);
				}
			}
		};
		build(*nodes.front());
		
		return Aligned::New(
			Alignment::Center, 
			Alignment::Start, 
			PopupPortal::New([](const PopupOpenContext&) {
					std::vector<std::unique_ptr<WidgetState>> out;
					out.emplace_back(new PopupMenuItem("Menu item 1", "F11", []() {}));
					out.emplace_back(new PopupMenuItem("Menu item 2", "F12", []() {}));
					out.emplace_back(new PopupSubmenuItem("Submenu 1", [](const PopupOpenContext&) {
						std::vector<std::unique_ptr<WidgetState>> out;
						out.emplace_back(new PopupMenuItem("Submenu item 1", "F2", []() {}));
						out.emplace_back(new PopupMenuItem("Submenu item 2", "F3", []() {}));
						return out;
					}));
					out.emplace_back(new PopupSubmenuItem("Submenu 2", [](const PopupOpenContext&) {
						std::vector<std::unique_ptr<WidgetState>> out;
						out.emplace_back(new PopupMenuItem("Submenu item 3", "F5", []() {}));
						out.emplace_back(new PopupMenuItem("Submenu item 4", "F6", []() {}));
						return out;
					}));
					return out;
				},
				Container::Build()
					.StyleClass("Frame")
					.SizeMode(AxisMode::Fixed, AxisMode::Expand)
					.Size(300, 0)
					.ClipContent()
					.Child(StatefulWidget::New(
						&scrollViewState_,
						Flexbox::Build()
							.DirectionColumn()
							.ExpandCrossAxis()
							.ExpandMainAxis(false)
							.Children(std::move(out))
							.New()
					))
					.New()
				)
		);
	}

	void SetRootDirectory(const std::path& dir) {
		root = dir;
		nodes.clear();
		auto* rootNode = nodes.emplace_back(new Node(root, 0, true, this)).get();

		for(const auto& file: std::directory_iterator(dir)) {
			if(!file.is_directory()) {
				continue;
			}
			const auto index = nodes.size();
			nodes.emplace_back(new Node(file.path(), 1, false, this));
			rootNode->childrenIndices.push_back(index);
		}
	}

	void ToggleNode(std::path filename) {
		auto& node = [&]()->Node& {
			for(auto& node: nodes) {
				if(node->filename == filename) {
					return *node;
				}
			}
			Assertf(false, "Node {} not found.", filename);
			return *nodes.front();
		}();
		node.isOpen = !node.isOpen;

		if(node.childrenIndices.empty()) {
			std::error_code ec;

			for(const auto& file: std::directory_iterator(node.filename, ec)) {
				if(!file.is_directory() || ec) {
					if(ec) {
						auto msg = ec.message();
						LOGF(Error, "Cannot process directory {}: {}", file.path(), ec.message());
					}
					continue;
				}
				const auto index = nodes.size();
				nodes.emplace_back(new Node(file.path(), node.indent + 1, false, this));
				node.childrenIndices.push_back(index);
			}
		}
		MarkNeedsRebuild();
	}

	class Node: public WidgetState {
	public:

		Node(const std::path& filename, int indent, bool isOpen, FilesystemView* parent) 
		 	: parent(parent)
			, isOpen(isOpen)
			, indent(indent)
			, filename(filename)
		{}

		std::unique_ptr<Widget> Build(std::unique_ptr<Widget>&&) override {
			return Button::Build()
				.SizeMode(AxisMode::Expand, AxisMode::Shrink)
				.StyleClass("FilesystemNode")
				.Text(std::string(indent, ' ')
					.append(isOpen ? "- " : "+ ")
					.append(filename.filename().string()))
				.Tooltip(filename.string())
				.OnPress([&](const ButtonEvent& e) { 
					if(e.button == MouseButton::ButtonLeft && !e.bPressed) {
						parent->ToggleNode(filename); 
						MarkNeedsRebuild();
					}
				})
				.New();
		}

		FilesystemView*  parent;
		std::path        filename;
		bool             isOpen;
		int				 indent;
		std::vector<u64> childrenIndices;
	};

private:
	std::path root;
	// Node tree stored in breadth first order
	std::vector<std::unique_ptr<Node>> nodes;
	ScrollViewState scrollViewState_;
};

int main(int argc, char* argv[]) {
	const auto commandLine = std::vector<std::string>(argv, argv + argc);
	const auto workingDir = std::filesystem::path(commandLine[0]).parent_path();
	logging::Init(workingDir.string());
	logging::SetLevel(logging::Level::All);

	g_Application = Application::Create("App", 1800, 900);
	SetDarkTheme();
	auto app = std::make_unique<FilesystemView>(std::path("G:\\"));
	g_Application->Parent(StatefulWidget::New(app.get()));

	while(g_Application->Tick());
}
