#pragma once
#include "framework.h"

namespace UI {

class Style;
class TextStyle;
class BoxStyle;
class LayoutStyle;
class Flexbox;
class Widget;
class PopupSpawner;
class Application;
class PopupState;
class Container;
struct PopupOpenContext;

// Called by the Application to create a PopupWindow
using PopupBuilderFunc = std::function<std::vector<std::unique_ptr<WidgetState>>(const PopupOpenContext&)>;

// Common state enums
// Used by button like widgets
struct StateEnum {
	static inline const StringID Normal{"Normal"};
	static inline const StringID Hovered{"Hovered"};
	static inline const StringID Pressed{"Pressed"};
	static inline const StringID Opened{"Opened"};
	static inline const StringID Selected{"Selected"};
};



/*
 * Displays not editable text
 */
class TextBox: public LayoutWidget {
	WIDGET_CLASS(TextBox, LayoutWidget)
public:
	static const inline StringID defaultStyleName{"Text"};

	static std::unique_ptr<TextBox> New(const std::string& text, StringID style = defaultStyleName) {
		return std::make_unique<TextBox>(text, style);
	}
	TextBox(const std::string& text, StringID style = defaultStyleName);

	void DebugSerialize(PropertyArchive& archive) override {
		Super::DebugSerialize(archive);
		// Show a text preview of 20 characters in the debug print
		constexpr auto previewSize = 20;
		std::string str;
		if(!text_.empty()) {
			if(text_.size() > previewSize) {
				str = text_.substr(0, previewSize);
				str.append("...");
			} else {
				str = text_;
			}
		}
		archive.PushStringProperty("Text", str);
	}

	bool OnEvent(IEvent* event) override;
	float2 OnLayout(const LayoutConstraints& event) override;

private:
	const StyleClass* style_;
	std::string       text_;
};





}  // namespace UI


