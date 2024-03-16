#pragma once
#include "font.h"
#include "runtime/base/string_id.h"
#include "runtime/core/core.h"
#include "runtime/core/util.h"

#include <set>
#include <variant>

namespace UI {

using Color = ColorU32;

namespace colors {

constexpr auto primaryDark   = Color::FromHex("#454C59");
constexpr auto secondaryDark = Color::FromHex("#21252B");
constexpr auto hoveredDark   = Color::FromHex("#586173");
constexpr auto pressedDark   = Color::FromHex("#7C88A1");

constexpr auto red   = Color::FromHex("#C44B37");
constexpr auto green = Color::FromHex("#48C43B");
constexpr auto blue  = Color::FromHex("#3951C4");
}  // namespace colors

constexpr std::string_view kStyleClassStateSeparator = ":";
constexpr auto             kDefaultMargins           = 5;
constexpr auto             kDefaultPaddings          = 5;
constexpr auto             kDefaultFontSize          = 13;

// Flags for selecting sides of a rect
enum class SideMask {
	None   = 0,
	Top    = 0x1,
	Bottom = 0x2,
	Left   = 0x4,
	Right  = 0x8,
	All    = Top | Bottom | Left | Right
};

// Flags for selecting corners of a rect
enum class CornerMask {
	None        = 0,
	TopLeft     = 0x1,
	TopRight    = 0x2,
	BottomLeft  = 0x4,
	BottomRight = 0x8,
	All         = TopLeft | TopRight | BottomLeft | BottomRight
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
	virtual ~Style() {}
	virtual Style* Copy() const = 0;
	StringID       name;
};



/*
 * Describes drawing a simple rectange with
 * background and borders
 */
class BoxStyle: public Style {
	DEFINE_CLASS_META(BoxStyle, Style)
public:
	using Sides = RectSides<u16>;

	BoxStyle(const BoxStyle* inParent = nullptr)
		: opacity(1.f)
		, rounding(0)
		, roundingMask(UI::CornerMask::All)
		, borderAsOutline(false) {
		if(inParent)
			*this = *inParent;
	}

	Style* Copy() const final { return new BoxStyle(*this); }

	BoxStyle& FillColor(std::string_view inHex) {
		backgroundColor = ColorU32::FromHex(inHex);
		return *this;
	}

	BoxStyle& BorderColor(std::string_view inHex) {
		borderColor = ColorU32::FromHex(inHex);
		return *this;
	}

	// Whole item opacity. From 0.f to 1.f
	BoxStyle& Opacity(float inOpacityNorm) {
		opacity = Math::Clamp(inOpacityNorm, 0.f, 1.f);
		return *this;
	}

	BoxStyle& Rounding(u8 inRounding) {
		rounding = inRounding;
		return *this;
	}

	BoxStyle& Borders(u8 inCommon) {
		borders = inCommon;
		return *this;
	}

	BoxStyle& Borders(u8 inHorizontal, u8 inVertical) {
		borders = {inHorizontal, inVertical};
		return *this;
	}

	BoxStyle& Borders(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
		borders = {inTop, inRight, inBottom, inLeft};
		return *this;
	}

public:
	static BoxStyle Default() {
		BoxStyle out{};
		out.borders         = 1;
		out.backgroundColor = "#737373";
		out.borderColor     = "#3B3B3B";
		return out;
	}

public:
	float      opacity;
	u8         rounding;
	CornerMask roundingMask;
	Sides      borders;
	Color      backgroundColor;
	Color      borderColor;
	// Draw border outside
	bool 	   borderAsOutline;
};



/*
 * Describes drawing a text
 */
class TextStyle: public Style {
	DEFINE_CLASS_META(TextStyle, Style)
public:
	TextStyle(const TextStyle* inParent)
		: font(nullptr)
		, fontSize(kDefaultFontSize)
		, fontWeightBold(false)
		, fontStyleItalic(false) {
		if(inParent)
			*this = *inParent;
	}

	Style* Copy() const final {
		return new TextStyle(*this);
	}

	float2 CalculateTextSize(const std::string& inText) const {
		return font->CalculateTextSize(inText, fontSize, fontWeightBold, fontStyleItalic);
	}

	TextStyle& Color(std::string_view inHex) {
		color = ColorU32::FromHex(inHex);
		return *this;
	}

	TextStyle& Size(u16 inDefaultSize) {
		fontSize = inDefaultSize;
		return *this;
	}

	TextStyle& FontFromFile(std::string_view inFilename) {
		filename = inFilename;
		return *this;
	}

	TextStyle& FontStyle(bool bItalic, bool bBold) {
		fontStyleItalic = bItalic;
		fontWeightBold  = bBold;
		return *this;
	}

public:
	std::string filename;
	UI::Font*   font;
	u32         fontSize;
	ColorU32    color;
	bool        fontWeightBold;
	bool        fontStyleItalic;
};



/*
 * Describes drawing an Image or Icon
 */
// class ImageStyle: public Style {
//	DEFINE_CLASS_META(ImageStyle, Style)
// public:

//	ImageStyle() { memset(this, 0, sizeof(*this)); }

//	//virtual void Draw(DrawList* inDrawList) override;

//	u32			ImageTexture;
//	u32			Padding;
//	u32			Border;

//	Color		TintColor;
//	Color		BackgroundColor;
//	Color		BorderColor;
//};

using Sides = RectSides<u32>;

/*
 * Handles separatly from other styles
 * because a change to visual style usually doesn't affect the layout of widgets
 * Motivation is to group widgets by their layout parameters independently from their visual representation
 * so multiple widget classes can have the same layout style but different visual styles
 */
class LayoutStyle: public Style {
	DEFINE_CLASS_META(LayoutStyle, Style)
public:
	LayoutStyle(const LayoutStyle* inParent = nullptr)
		: minHeight(0)
		, minWidth(0)
		, paddings(kDefaultPaddings)
		, margins(kDefaultMargins) {
		if(inParent)
			*this = *inParent;
	}

	Style* Copy() const final {
		return new LayoutStyle(*this);
	}

	LayoutStyle& Paddings(u8 inCommon) {
		paddings = inCommon;
		return *this;
	}

	LayoutStyle& Paddings(u8 inHorizontal, u8 inVertical) {
		paddings = {inHorizontal, inVertical};
		return *this;
	}

	LayoutStyle& Paddings(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
		paddings = {inTop, inRight, inBottom, inLeft};
		return *this;
	}

	LayoutStyle& Margins(u8 inCommon) {
		margins = inCommon;
		return *this;
	}

	LayoutStyle& Margins(u8 inHorizontal, u8 inVertical) {
		margins = {inHorizontal, inVertical};
		return *this;
	}

	LayoutStyle& Margins(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
		margins = {inTop, inRight, inBottom, inLeft};
		return *this;
	}

public:
	float minHeight;
	float minWidth;
	Sides paddings;
	Sides margins;
};

template<typename T>
concept StyleParameterType = std::same_as<T, u32> || std::same_as<T, s32> || std::same_as<T, float>;

using StyleParameterVariant = std::variant<u32, s32, float>;


/*
 * Containts a collection of named style parameters used by widgets
 * A widget expects certain parameters to draw itself
 * E.g. a Button expects a BoxStyle to draw background and borders
 * A Text widget expects a TextStyle with specified font and size
 */
class StyleClass {
public:
	StyleClass(StringID inName, const StyleClass* inParent = nullptr)
		: m_Name(inName)
		, m_Parent(inParent) {}

	StyleClass(const StyleClass& inStyle)
		: m_Name(inStyle.m_Name)
		, m_Parent(inStyle.m_Parent) {
		m_Styles.reserve(inStyle.m_Styles.size());

		for(const auto& styleToCopy: inStyle.m_Styles) {
			m_Styles.emplace_back(styleToCopy->Copy());
		}
	}

	template<class StyleType>
	StyleType& Add(std::string_view inName = "", std::string_view inStyleCopy = "") {
		auto name          = StringID(inName.data());
		auto copyStyleName = StringID(inStyleCopy.data());

		const StyleType* copyStyle = nullptr;

		if(!copyStyleName.Empty()) {
			copyStyle = Find<StyleType>(copyStyleName);
			Assertm(copyStyle, "Copy style with specified name and class not found");
		}

		auto out  = new StyleType(copyStyle);
		out->name = name;
		m_Styles.emplace_back(out);
		return *out;
	}

	template<StyleParameterType T>
	void AddParameter(std::string_view inName, T inVal) {
		auto name = StringID(inName.data());

		m_Parameters.emplace_back(StyleParameterVariant(inVal));
	}

	template<class StyleType>
	const StyleType* Find(StringID inName = "") const {
		const StyleType* out = nullptr;

		for(auto& style: m_Styles) {
			if(style->IsA<StyleType>() && (inName == style->name || inName.Empty())) {
				out = style->Cast<StyleType>();
				break;
			}
		}

		if(!out && m_Parent) {
			out = m_Parent->Find<StyleType>(inName);
		}
		Assertf(out, "Style class '{}' does not contain a style of type '{}' and name '{}'", *m_Name, StyleType::GetStaticClassName(), inName.String());
		return out;
	}

	// If style with specified class and name not found returns default style with empty name
	template<class StyleType>
	const StyleType* FindOrDefault(StringID inName = "") const {
		const StyleType* out = nullptr;
		// If we are the fallback style ignore name selector
		const auto name = m_Name.Empty() ? m_Name : inName;

		for(auto& style: m_Styles) {
			if(style->IsA<StyleType>() && (name == style->name || name.Empty())) {
				out = style->Cast<StyleType>();
				break;
			}
		}

		if(!out && m_Parent) {
			out = m_Parent->Find<StyleType>(name);
		}

		if(!out) {
			// Return fallback
			LOGF(Verbose, "Style [{}:{}] not found. Using fallback style.", StyleType::GetStaticClassName(), name);
			return m_Parent->FindOrDefault<StyleType>();
		}
		return out;
	}

	// Add styles from other style class overriding existent
	void Merge(const StyleClass& inStyle) {
		std::ranges::for_each(inStyle.m_Styles, [this](const std::unique_ptr<Style>& inStyle) {
			for(auto& style: m_Styles) {
				if(style->GetClass() == inStyle->GetClass() && (style->name == inStyle->name)) {
					style.reset(inStyle->Copy());
					return;
				}
			}
			m_Styles.emplace_back(inStyle->Copy());
		});
	}

public:
	StringID                            m_Name;
	const StyleClass*                   m_Parent;
	std::vector<std::unique_ptr<Style>> m_Styles;
	std::vector<StyleParameterVariant>  m_Parameters;
};


/*
 * A database of style parameters used to draw a widget
 * Each widget uses a one StyleClass instance from the database
 */
class Theme {
public:
	Theme(const Theme&)            = delete;
	Theme& operator=(const Theme&) = delete;

	Theme(u8 inDefaultFontSize = kDefaultFontSize) {
		// Create default fallback styles
		auto& fallback = m_Styles.emplace("", StyleClass("")).first->second;
		fallback.Add<LayoutStyle>();
		// Purple color
		fallback.Add<BoxStyle>().FillColor("#FF00EE");

		auto& text    = fallback.Add<TextStyle>();
		text.fontSize = inDefaultFontSize;
		text.color    = Color::FromHex("#ffffff");
		m_Fallback    = &fallback;

		auto* fontDefault = &*m_Fonts.emplace_back(Font::FromInternal());
		fontDefault->RasterizeFace(inDefaultFontSize);
		text.font = fontDefault;
	}

	// Adds a new style or returns existing
	StyleClass& Add(std::string_view inStyleName, std::string_view inParentStyle = "") {
		if(inStyleName.empty())
			return *m_Fallback;

		auto              name       = StringID(inStyleName.data());
		auto              parentName = StringID(inParentStyle.data());
		const StyleClass* parent     = m_Fallback;

		if(!parentName.Empty()) {
			parent = Find(parentName);
			Assertf(parent, "Cannot find parent style with the name {}", inParentStyle);
		}
		auto [it, bExists] = m_Styles.emplace(name, StyleClass(name, parent));
		return it->second;
	}

	// Merges two themes together
	void Merge(const Theme* inTheme, bool bOverride = true) {
		for(const auto& [id, style]: inTheme->m_Styles) {
			auto thisStyleEntry = m_Styles.find(id);

			if(thisStyleEntry == m_Styles.end()) {
				m_Styles.emplace(id, style);

			} else if(bOverride) {
				thisStyleEntry->second.Merge(style);
			}
		}
	}

	// Gathers all fonts from styles and try to load them
	// @param outFonts - Fonts used by the theme.
	// The caller is responsible for building the atlas for rendering these fonts
	void RasterizeFonts(std::vector<Font*>* outFonts) {
		// Load fonts
		for(auto& [selector, styleClass]: m_Styles) {
			for(auto& style: styleClass.m_Styles) {
				if(auto* textStyle = style->Cast<TextStyle>()) {

					if(textStyle->font) {
						if(textStyle->font->NeedsRebuild()) {
							outFonts->push_back(textStyle->font);
						}

					} else if(textStyle->filename.empty()) {
						// If empty set default
						textStyle->font = m_Fonts.front().get();
						textStyle->font->RasterizeFace(textStyle->fontSize);
						outFonts->push_back(textStyle->font);

					} else {
						textStyle->font = Font::FromFile(textStyle->filename);
						Assertf(textStyle->font, "Font with filename {} not found", textStyle->filename);

						if(textStyle->font) {
							m_Fonts.emplace_back(textStyle->font);
							textStyle->font->RasterizeFace(textStyle->fontSize);
						}
					}
				}
			}
		}
	}

	// Default internal font. Currently ImGui internal
	Font* GetDefaultFont() { return m_Fallback->Find<TextStyle>()->font; }

	const StyleClass* Find(StringID inStyleClass) const {
		auto it = m_Styles.find(inStyleClass);

		if(it == m_Styles.end()) {
			LOGF(Verbose, "Style with the name {} not found. Using fallback style.", inStyleClass);
			return m_Fallback;
		}
		return it != m_Styles.end() ? &it->second : nullptr;
	}

private:
	StyleClass*                    m_Fallback;
	std::map<StringID, StyleClass> m_Styles;
	// All fonts used by this theme
	std::vector<std::unique_ptr<Font>> m_Fonts;
};
}  // namespace UI