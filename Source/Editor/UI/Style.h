#pragma once
#include "Runtime/Core/Core.h"
#include "Runtime/Core/Util.h"
#include "Font.h"

namespace UI {

	using Color = ColorU32;

	namespace Colors {

		constexpr auto PrimaryDark		= Color("#454C59");
		constexpr auto SecondaryDark	= Color("#21252B");
		constexpr auto HoveredDark		= Color("#586173");
		constexpr auto PressedDark		= Color("#7C88A1");

		constexpr auto Red				= Color("#C44B37");
		constexpr auto Green			= Color("#48C43B");
		constexpr auto Blue				= Color("#3951C4");
	}

	// Flags for selecting sides of a rect
	enum class SideMask {
		None = 0,
		Top = 0x1,
		Bottom = 0x2,
		Left = 0x4,
		Right = 0x8,
		All = Top | Bottom | Left | Right
	};

	// Flags for selecting corners of a rect
	enum class CornerMask {
		None = 0,
		TopLeft = 0x1,
		TopRight = 0x2,
		BottomLeft = 0x4,
		BottomRight = 0x8,
		All = TopLeft | TopRight | BottomLeft | BottomRight
	};

	DEFINE_ENUM_FLAGS_OPERATORS(SideMask)
	DEFINE_ENUM_FLAGS_OPERATORS(CornerMask)


	/*
	* Containts parameters for drawing specific shape
	* Widget can use multiple those objects to draw itself
	*/
	class Style {
		DEFINE_ROOT_CLASS_META(Style)
	public:

		virtual ~Style() = default;
		//virtual void Draw(DrawList* inDrawList) = 0;
	};



	/*
	* Describes drawing a simple rectange with
	* background and borders
	*/
	class BoxStyle: public Style {
		DEFINE_CLASS_META(BoxStyle, Style)
	public:

		using Sides = Util::Sides;

		BoxStyle() { memset(this, 0, sizeof(*this)); }

		//virtual void Draw(DrawList* inDrawList) override;

		float		Opacity;
		float		MinHeight;
		float		MinWidth;
		u8			Rounding;

		CornerMask	RoundingMask;
		Sides		Borders;
		Sides		Paddings;
		Sides		Margins;

		Color		BackgroundColor;
		Color		BorderColor;
	};

	/*
	* Describes drawing a text
	*/
	class TextStyle: public Style {
		DEFINE_CLASS_META(TextStyle, Style)
	public:

		TextStyle() { memset(this, 0, sizeof(*this)); }

		float2 CalculateTextSize(const std::string& inText) const { 
			return Font->CalculateTextSize(inText, FontSize, FontWeightBold, FontStyleItalic);
		}

		//virtual void Draw(DrawList* inDrawList) override;

		UI::Font*	Font;
		u32			FontSize;
		Color		Color;
		bool		FontWeightBold;
		bool		FontStyleItalic;
	};

	/*
	* Describes drawing an Image or Icon
	*/
	class ImageStyle: public Style {
		DEFINE_CLASS_META(ImageStyle, Style)
	public:

		ImageStyle() { memset(this, 0, sizeof(*this)); }

		//virtual void Draw(DrawList* inDrawList) override;

		u32			ImageTexture;
		u32			Padding;
		u32			Border;

		Color		TintColor;
		Color		BackgroundColor;
		Color		BorderColor;
	};

	/*
	* Contains list of styles applied to widgets
	* 
	* 
	* System::OnDirty(this);
	*
	*
	*	// Widget code
	*	return RenderObject("Button:Hovered").
	*		BoxClipper(Super::GetSize() - Super::GetPaddings()).
	*		Box("MainShape", Super::GetSize()).
	*		Box("SecondaryShape", Super::GetSize() - 10.f, 5.f);
	*
	*
	*	renderObject.AddBox("MainShape", Super::GetRect());
	*
	*	// Style code
	*	StyleBuilder builder;
	*
	*	// Layout parameters for all widgets
	*	builder.Layout("").Margins(10, 10).Paddings(10, 10);
	*	// Layout parameters for class "Button"
	*	builder.Layout("Button").Margins(5, 5).Paddings(10, 10).MaxWidth(50);
	*
	*	// Style parameters for class:state:shape
	*	builder.Box("Button:Normal:MainShape").FillColor("#454545").Border(1.f).BorderColor("#343434");
	*	builder.Box("Button:Normal:SecondShape").FillColor("#454545").Border(1.f).BorderColor("#343434");
	*
	*	builder.Box("Button:Hovered:MainShape", "Button:Normal:MainShape").FillColor("#505050").Border(1.f).BorderColor("#343434");
	*	builder.Box("Button:Hovered:SecondShape", "Button:Normal:SecondShape").FillColor("#505050").Border(1.f).BorderColor("#343434");
	*
	*	builder.Text("Text").Color("#ffffff").Size(13).FontFromDisk("res/fonts/editor_font.ttf");
	*	builder.Text("Text:Inactive", "Text").Color("#dddddd");
	*	builder.Box("Text:Selected:SelectionBox").FillColor("#1010dd");
	*
	*	Theme::SubmitStyles(builder);
	* 
	*/
	class Theme {
	public:

		Theme() {
			Font.reset(CreateFontInternal());
			Font->RasterizeFace(13);

			DefaultTextStyle.reset(new TextStyle());

			DefaultTextStyle->Font = Font.get();
			DefaultTextStyle->FontSize = 13;
			DefaultTextStyle->Color = "#ffffff";
		}

		std::unique_ptr<Font>		Font;
		std::unique_ptr<TextStyle>	DefaultTextStyle;

		std::vector<Style*>			m_StyleList;
	};
}

