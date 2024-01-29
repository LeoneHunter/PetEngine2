#pragma once
#include "Runtime/Core/Core.h"
#include "Runtime/Core/Util.h"
#include "Font.h"

#include <set>

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

	constexpr std::string_view kStyleClassStateSeparator = ":";
	constexpr auto kDefaultMargins = 5;
	constexpr auto kDefaultPaddings = 5;
	constexpr auto kDefaultFontSize = 5;

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
		std::string Selector;
	};



	/*
	* Describes drawing a simple rectange with
	* background and borders
	*/
	class BoxStyle: public Style {
		DEFINE_CLASS_META(BoxStyle, Style)
	public:

		using Sides = RectSides<u16>;

		BoxStyle(std::string_view inSelector, const BoxStyle* inParent = nullptr) 
			: Opacity(1.f)
			, Rounding(0)
			, RoundingMask(UI::CornerMask::All)
		{
			if(inParent) {
				*this = *inParent;				
			}
			Style::Selector = inSelector;
		}

	public:
		float		Opacity;
		u8			Rounding;
		CornerMask	RoundingMask;
		Sides		Borders;
		Color		BackgroundColor;
		Color		BorderColor;
	};



	/*
	* Describes drawing a text
	*/
	class TextStyle: public Style {
		DEFINE_CLASS_META(TextStyle, Style)
	public:

		TextStyle(std::string_view inSelector, const TextStyle* inParent = nullptr) 
			: Font(nullptr)
			, FontSize(kDefaultFontSize)
			, FontWeightBold(false)
			, FontStyleItalic(false) 
		{
			if(inParent) {
				*this = *inParent;
			}
			Style::Selector = inSelector;
		}

		float2 CalculateTextSize(const std::string& inText) const {
			return Font->CalculateTextSize(inText, FontSize, FontWeightBold, FontStyleItalic);
		}

	public:
		std::string	Filename;
		UI::Font*	Font;
		u32			FontSize;
		Color		Color;
		bool		FontWeightBold;
		bool		FontStyleItalic;
	};



	/*
	* Describes drawing an Image or Icon
	*/
	//class ImageStyle: public Style {
	//	DEFINE_CLASS_META(ImageStyle, Style)
	//public:

	//	ImageStyle() { memset(this, 0, sizeof(*this)); }

	//	//virtual void Draw(DrawList* inDrawList) override;

	//	u32			ImageTexture;
	//	u32			Padding;
	//	u32			Border;

	//	Color		TintColor;
	//	Color		BackgroundColor;
	//	Color		BorderColor;
	//};



	/*
	* Handles separatly from other styles
	* because a change to visual style usually doesn't affect the layout of widgets
	* Motivation is to group widgets by their layout parameters independently from their visual representation
	* so multiple widget classes can have the same layout style but different visual styles
	*/
	class LayoutStyle: public Style {
		DEFINE_CLASS_META(LayoutStyle, Style)
	public:

		using Sides = RectSides<u16>;

		LayoutStyle(std::string_view inSelector, const LayoutStyle* inParent = nullptr) 
			: MinHeight(0)
			, MinWidth(0)
			, Paddings(kDefaultPaddings)
			, Margins(kDefaultMargins)
		{
			if(inParent) {
				*this = *inParent;
			}
			Style::Selector = inSelector;
		}

	public:
		float MinHeight;
		float MinWidth;
		Sides Paddings;
		Sides Margins;
	};



	/*
	* Contains list of styles applied to widgets
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
	*	builder.Box("Button:Hovered:MainShape", "Button:Normal:MainShape").FillColor("#505050");
	*	builder.Box("Button:Hovered:SecondShape", "Button:Normal:SecondShape").FillColor("#505050");
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

		struct BoxStyler {

			BoxStyler(UI::BoxStyle* inStyle)
				: m_Style(inStyle) {}

			BoxStyler& FillColor(std::string_view inColorHash) {
				m_Style->BackgroundColor = ColorU32(inColorHash);
				return *this;
			}

			BoxStyler& BorderColor(std::string_view inColorHash) {
				m_Style->BorderColor = ColorU32(inColorHash);
				return *this;
			}

			// Whole item opacity. From 0.f to 1.f
			BoxStyler& Opacity(float inOpacityNorm) {
				m_Style->Opacity = Math::Clamp(inOpacityNorm, 0.f, 1.f);
				return *this;
			}

			BoxStyler& Rounding(u8 inRounding) {
				m_Style->Rounding = inRounding;
				return *this;
			}

			BoxStyler& Borders(u8 inCommon) {
				m_Style->Borders = inCommon;
				return *this;
			}

			BoxStyler& Borders(u8 inHorizontal, u8 inVertical) {
				m_Style->Borders = {inHorizontal, inVertical};
				return *this;
			}

			BoxStyler& Borders(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
				m_Style->Borders = {inTop, inRight, inBottom, inLeft};
				return *this;
			}

			UI::BoxStyle* m_Style;
		};

		struct TextStyler {

			TextStyler(UI::TextStyle* inStyle)
				: m_Style(inStyle)
			{}

			TextStyler& Color(std::string_view inColorHash) {
				m_Style->Color = ColorU32(inColorHash);
				return *this;
			}

			TextStyler& Size(u16 inDefaultSize) {
				m_Style->FontSize = inDefaultSize;
				return *this;
			}

			TextStyler& FontFromFile(std::string_view inFilename) {
				m_Style->Filename = inFilename;
				return *this;
			}

			TextStyler& FontStyle(bool bItalic, bool bBold) {
				m_Style->FontStyleItalic = bItalic;
				m_Style->FontWeightBold = bBold;
				return *this;
			}

			UI::TextStyle* m_Style;
		};

		struct LayoutStyler {

			LayoutStyler(UI::LayoutStyle* inStyle)
				: m_Style(inStyle) {}

			LayoutStyler& Paddings(u8 inCommon) {
				m_Style->Paddings = inCommon;
				return *this;
			}

			LayoutStyler& Paddings(u8 inHorizontal, u8 inVertical) {
				m_Style->Paddings = {inHorizontal, inVertical};
				return *this;
			}

			LayoutStyler& Paddings(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
				m_Style->Paddings = {inTop, inRight, inBottom, inLeft};
				return *this;
			}

			LayoutStyler& Margins(u8 inCommon) {
				m_Style->Margins = inCommon;
				return *this;
			}

			LayoutStyler& Margins(u8 inHorizontal, u8 inVertical) {
				m_Style->Margins = {inHorizontal, inVertical};
				return *this;
			}

			LayoutStyler& Margins(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
				m_Style->Margins = {inTop, inRight, inBottom, inLeft};
				return *this;
			}

			UI::LayoutStyle* m_Style;
		};

	public:

		Theme(u8 inDefaultFontSize = kDefaultFontSize) {
			// ImGui font
			m_Fonts.emplace_back(Font::CreateInternal());
			m_Fonts.back()->RasterizeFace(inDefaultFontSize);
		}

		LayoutStyler Layout(const std::string& inSelector, const std::string& inParentStyleSelector = "") {
			auto selector = inSelector;
			Assertm(!m_LayoutStyles.contains(selector), "Style with such selector already created");
			UI::LayoutStyle* parent = nullptr;

			if(!inParentStyleSelector.empty()) {
				parent = FindStyle<LayoutStyle>(inParentStyleSelector);
				Assertm(parent, "Parent style with specified selector and class not found");
			}

			if(selector.empty()) {
				selector = "$FALLBACK_LAYOUT$";
			}
			auto out = new UI::LayoutStyle(selector, parent);
			m_LayoutStyles.emplace(selector, out);
			return {out};
		}

		BoxStyler Box(const std::string& inSelector, const std::string& inParentStyleSelector = "") {
			return Styler<BoxStyler, UI::BoxStyle>(inSelector, inParentStyleSelector);
		}

		TextStyler Text(const std::string& inSelector, const std::string& inParentStyleSelector = "") {
			return Styler<TextStyler, UI::TextStyle>(inSelector, inParentStyleSelector);
		}

	public:

		constexpr static auto kFallbackBoxStyleName = "$FALLBACK_BOX$";
		constexpr static auto kFallbackLayoutStyleName = "$FALLBACK_LAYOUT$";
		constexpr static auto kFallbackTextStyleName = "$FALLBACK_TEXT$";

		// Merges two themes together overriding current styles
		void Merge(const Theme* inOther, bool bOverride = true) {

			for(const auto& otherStyle : inOther->m_Styles) {
				auto it = m_Styles.find(otherStyle.first);

				if(it == m_Styles.end()) {
					m_Styles.emplace(otherStyle.first, otherStyle.second.get());

				} else if(bOverride) {
					Assertm(it->second->GetClass() == otherStyle.second->GetClass(), "Two styles with the same key but different classes found");

					if(auto boxStyle = it->second->As<BoxStyle>()) {
						*boxStyle = *(otherStyle.second->As<BoxStyle>());

					} else if(auto textStyle = it->second->As<TextStyle>()) {
						*textStyle = *(otherStyle.second->As<TextStyle>());

					} else {
						Assertm(false, "Unknown class");
					}
				}
			}

			for(const auto& otherStyle : inOther->m_LayoutStyles) {
				auto it = m_LayoutStyles.find(otherStyle.first);

				if(it == m_LayoutStyles.end()) {
					m_LayoutStyles.emplace(otherStyle.first, otherStyle.second.get());

				} else if(bOverride) {
					*(it->second->As<LayoutStyle>()) = *(otherStyle.second->As<LayoutStyle>());
				}
			}
		}

		/*
		* Tries to find a style with specified selectors and class
		* If exact match not found, searches only for class
		* If class also not found returns fallback style
		*/
		template<typename T>
		T* FindStyle(const std::string& inClass, const std::string& inState = "") {
			if constexpr(std::same_as<T, BoxStyle> || std::same_as<T, TextStyle>) {
				return FindStyleInternal<T>(inClass, inState, m_Styles);
			} else if constexpr(std::same_as<T, LayoutStyle>) {
				return FindStyleInternal<T>(inClass, inState, m_LayoutStyles);
			}
			Assert("Unknown style type");
			return nullptr;
		}

		// Gathers all fonts from styles and try to load them
		void Finalize() {
			// Ensure that we have empty style as a backup			
			if(m_Styles.find(kFallbackBoxStyleName) == m_Styles.end()) {
				m_Styles.emplace(kFallbackBoxStyleName, new BoxStyle(kFallbackBoxStyleName));
			}

			if(m_Styles.find(kFallbackTextStyleName) == m_Styles.end()) {
				auto defaultStyle = new TextStyle(kFallbackTextStyleName);
				defaultStyle->Color = "#eeeeee";
				defaultStyle->FontSize = kDefaultFontSize;
				defaultStyle->Font = m_Fonts.front().get();
				m_Styles.emplace(kFallbackTextStyleName, defaultStyle);
			}

			if(m_LayoutStyles.find(kFallbackLayoutStyleName) == m_LayoutStyles.end()) {
				auto defaultStyle = new LayoutStyle(kFallbackLayoutStyleName);
				defaultStyle->Paddings = kDefaultPaddings;
				defaultStyle->Margins = kDefaultMargins;
				m_LayoutStyles.emplace(kFallbackLayoutStyleName, defaultStyle);
			}			
			
			// Load fonts
			for(auto& mapEntry : m_Styles) {
				Style* style = mapEntry.second.get();

				if(auto* textStyle = style->As<TextStyle>()) {
					// If empty set default
					if(textStyle->Filename.empty()) {
						textStyle->Font = m_Fonts.front().get();
						textStyle->Font->RasterizeFace(textStyle->FontSize);

					} else {
						textStyle->Font = Font::CreateFromFile(textStyle->Filename);
						Assertf(textStyle->Font, "Font with filename {} not found", textStyle->Filename);

						if(textStyle->Font) {
							m_Fonts.emplace_back(textStyle->Font);
							textStyle->Font->RasterizeFace(textStyle->FontSize);
						}
					}
				}
			}
		}

		void GetFonts(std::vector<Font*>* outFonts) {
			for(auto& font : m_Fonts) {
				outFonts->push_back(font.get());
			}
		}

		Font* GetDefaultFont() {
			return m_Fonts.front().get();
		}

	private:

		template<class StylerType, class StyleType>
		StylerType Styler(const std::string& inSelector, const std::string& inParentStyleSelector = "") {
			auto selector = inSelector;
			Assertm(!m_Styles.contains(selector), "Style with such selector already created");
			StyleType* parent = nullptr;

			if(!inParentStyleSelector.empty()) {
				parent = FindStyle<StyleType>(inParentStyleSelector);
				Assertm(parent, "Parent style with specified selector and class not found");
			}

			if(selector.empty()) {

				if constexpr(std::same_as<StyleType, BoxStyle>) {
					selector = kFallbackBoxStyleName;
				} else if constexpr(std::same_as<StyleType, TextStyle>) {
					selector = kFallbackTextStyleName;
				}
			}
			auto out = new StyleType(selector, parent);
			m_Styles.emplace(selector, out);
			return {out};
		}

		template<class StyleType, class MapType>
		StyleType* FindStyleInternal(const std::string& inClass, const std::string& inState, MapType& inMap) {
			auto selector = std::string(inClass);

			if(!inState.empty()) {
				selector.append(kStyleClassStateSeparator).append(inState);
			}
			auto it = inMap.find(selector);

			if(it != inMap.end()) {
				return it->second->As<StyleType>();
			}
			it = inMap.find(inClass);

			if(it != inMap.end()) {
				return it->second->As<StyleType>();
			}

			std::string fallbackStyleSelector;

			if constexpr(std::same_as<StyleType, BoxStyle>) {
				fallbackStyleSelector = kFallbackBoxStyleName;
			} else if constexpr(std::same_as<StyleType, TextStyle>) {
				fallbackStyleSelector = kFallbackTextStyleName;
			} else if constexpr(std::same_as<StyleType, LayoutStyle>) {
				fallbackStyleSelector = kFallbackLayoutStyleName;
			}

			it = inMap.find(fallbackStyleSelector);
			Assertm(it != inMap.end(), "Theme has to be finalized");
			return it->second->As<StyleType>();
		}

	private:
		std::map<std::string, std::unique_ptr<LayoutStyle>>	m_LayoutStyles;
		std::map<std::string, std::unique_ptr<Style>>		m_Styles;
		// All fonts used by this theme
		std::vector<std::unique_ptr<Font>>					m_Fonts;
	};
}