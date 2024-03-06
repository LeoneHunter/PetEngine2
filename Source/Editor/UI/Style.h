#pragma once
#include "Runtime/Core/Core.h"
#include "Runtime/Core/Util.h"
#include "Runtime/Base/StringID.h"
#include "Font.h"

#include <set>
#include <variant>

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
		virtual ~Style() {}
		virtual Style* Copy() const = 0;
		StringID Name;
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
			: Opacity(1.f)
			, Rounding(0)
			, RoundingMask(UI::CornerMask::All)
			, BorderAsOutline(false)
		{
			if(inParent) *this = *inParent;
		}

		Style* Copy() const final {
			return new BoxStyle(*this);
		}

		BoxStyle& SetFillColor(std::string_view inColorHash) {
			BackgroundColor = ColorU32(inColorHash);
			return *this;
		}

		BoxStyle& SetBorderColor(std::string_view inColorHash) {
			BorderColor = ColorU32(inColorHash);
			return *this;
		}

		// Whole item opacity. From 0.f to 1.f
		BoxStyle& SetOpacity(float inOpacityNorm) {
			Opacity = Math::Clamp(inOpacityNorm, 0.f, 1.f);
			return *this;
		}

		BoxStyle& SetRounding(u8 inRounding) {
			Rounding = inRounding;
			return *this;
		}

		BoxStyle& SetBorders(u8 inCommon) {
			Borders = inCommon;
			return *this;
		}

		BoxStyle& SetBorders(u8 inHorizontal, u8 inVertical) {
			Borders = {inHorizontal, inVertical};
			return *this;
		}

		BoxStyle& SetBorders(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
			Borders = {inTop, inRight, inBottom, inLeft};
			return *this;
		}

	public:

		static BoxStyle Default() {
			BoxStyle out{};
			out.Borders = 1;
			out.BackgroundColor = "#737373";
			out.BorderColor = "#3B3B3B";
			return out;
		}

	public:
		float		Opacity;
		u8			Rounding;
		CornerMask	RoundingMask;
		Sides		Borders;
		Color		BackgroundColor;
		Color		BorderColor;
		// Draw border outside
		bool		BorderAsOutline;
	};



	/*
	* Describes drawing a text
	*/
	class TextStyle: public Style {
		DEFINE_CLASS_META(TextStyle, Style)
	public:

		TextStyle(const TextStyle* inParent) 
			: Font(nullptr)
			, FontSize(kDefaultFontSize)
			, FontWeightBold(false)
			, FontStyleItalic(false)
		{
			if(inParent) *this = *inParent;
		}

		Style* Copy() const final {
			return new TextStyle(*this);
		}

		float2 CalculateTextSize(const std::string& inText) const {
			return Font->CalculateTextSize(inText, FontSize, FontWeightBold, FontStyleItalic);
		}

		TextStyle& SetColor(std::string_view inColorHash) {
			Color = ColorU32(inColorHash);
			return *this;
		}

		TextStyle& SetSize(u16 inDefaultSize) {
			FontSize = inDefaultSize;
			return *this;
		}

		TextStyle& SetFontFromFile(std::string_view inFilename) {
			Filename = inFilename;
			return *this;
		}

		TextStyle& SetFontStyle(bool bItalic, bool bBold) {
			FontStyleItalic = bItalic;
			FontWeightBold = bBold;
			return *this;
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

		using Sides = RectSides<u32>;

		LayoutStyle(const LayoutStyle* inParent = nullptr)
			: MinHeight(0)
			, MinWidth(0)
			, Paddings(kDefaultPaddings)
			, Margins(kDefaultMargins)
		{
			if(inParent) *this = *inParent;
		}

		Style* Copy() const final {
			return new LayoutStyle(*this);
		}
		
		LayoutStyle& SetPaddings(u8 inCommon) {
			Paddings = inCommon;
			return *this;
		}

		LayoutStyle& SetPaddings(u8 inHorizontal, u8 inVertical) {
			Paddings = {inHorizontal, inVertical};
			return *this;
		}

		LayoutStyle& SetPaddings(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
			Paddings = {inTop, inRight, inBottom, inLeft};
			return *this;
		}

		LayoutStyle& SetMargins(u8 inCommon) {
			Margins = inCommon;
			return *this;
		}

		LayoutStyle& SetMargins(u8 inHorizontal, u8 inVertical) {
			Margins = {inHorizontal, inVertical};
			return *this;
		}

		LayoutStyle& SetMargins(u8 inTop, u8 inRight, u8 inBottom, u8 inLeft) {
			Margins = {inTop, inRight, inBottom, inLeft};
			return *this;
		}

	public:
		float MinHeight;
		float MinWidth;
		Sides Paddings;
		Sides Margins;
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
			, m_Parent(inParent)
		{}

		StyleClass(const StyleClass& inStyle)
			: m_Name(inStyle.m_Name)
			, m_Parent(inStyle.m_Parent)
		{
			m_Styles.reserve(inStyle.m_Styles.size());

			for(const auto& styleToCopy : inStyle.m_Styles) {
				m_Styles.emplace_back(styleToCopy->Copy());
			}
		}

		template<class StyleType>
		StyleType& Add(std::string_view inName = "", std::string_view inStyleCopy = "") {
			auto name = StringID(inName.data());
			auto copyStyleName = StringID(inStyleCopy.data());

			const StyleType* copyStyle = nullptr;

			if(!copyStyleName.Empty()) {
				copyStyle = Find<StyleType>(copyStyleName);
				Assertm(copyStyle, "Copy style with specified name and class not found");
			}

			auto out = new StyleType(copyStyle);
			out->Name = name;
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

			for(auto& style : m_Styles) {

				if(style->IsA<StyleType>() && (inName == style->Name || inName.Empty())) {
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

		template<class StyleType>
		const StyleType* FindOrDefault(StringID inName, const StyleType* inDefault) const {
			const StyleType* out = nullptr;

			for(auto& style : m_Styles) {

				if(style->IsA<StyleType>() && (inName == style->Name || inName.Empty())) {
					out = style->Cast<StyleType>();
					break;
				}
			}

			if(!out && m_Parent) {
				out = m_Parent->Find<StyleType>(inName);
			}

			if(!out) {
				return inDefault;
			}
			return out;
		}

		// Add styles from other style class overriding existent
		void Merge(const StyleClass& inStyle) {

			std::ranges::for_each(inStyle.m_Styles, [this](const std::unique_ptr<Style>& inStyle) {

				for(auto& style : m_Styles) {

					if(style->GetClass() == inStyle->GetClass() && (style->Name == inStyle->Name)) {
						style.reset(inStyle->Copy());
						return;
					}
				}
				m_Styles.emplace_back(inStyle->Copy());
			});
		}		

	public:
		StringID							m_Name;
		const StyleClass*					m_Parent;
		std::vector<std::unique_ptr<Style>>	m_Styles;
		std::vector<StyleParameterVariant>  m_Parameters;
	};


	/*
	* A database of style parameters used to draw a widget
	* Each widget uses a one StyleClass instance from the database
	*/
	class Theme {
	public:

		Theme(const Theme&) = delete;
		Theme& operator=(const Theme&) = delete;

		// Creates default light theme
		static Theme* DefaultLight() {
			auto* theme = new UI::Theme();

			// Flexbox and Splitbox
			{
				auto& containerStyle = theme->Add("Container");
				containerStyle.Add<LayoutStyle>().SetMargins(0).SetPaddings(5);
				containerStyle.Add<BoxStyle>().SetFillColor("#454545").SetBorders(0).SetRounding(0);

				theme->Add("Flexbox", "Container");
				theme->Add("Splitbox", "Container");
			}

			// Text
			{
				theme->Add("Text").Add<TextStyle>().SetColor("#ffffff").SetSize(13);
			}
			
			// Button
			{
				auto& buttonStyle = theme->Add("Button"); {
					buttonStyle.Add<LayoutStyle>("Normal").SetMargins(5).SetPaddings(10);
					buttonStyle.Add<LayoutStyle>("Hovered", "Normal");
					buttonStyle.Add<LayoutStyle>("Pressed", "Normal");

					buttonStyle.Add<BoxStyle>("Normal").SetFillColor("#454545").SetBorders(0).SetBorderColor("#343434").SetRounding(6);
					buttonStyle.Add<BoxStyle>("Hovered", "Normal").SetFillColor("#808080");
					buttonStyle.Add<BoxStyle>("Pressed", "Normal").SetFillColor("#eeeeee");
				}
			}
			return theme;
		}

		Theme(u8 inDefaultFontSize = kDefaultFontSize) {
			// ImGui font
			m_Fonts.emplace_back(Font::CreateInternal());
			m_Fonts.back()->RasterizeFace(inDefaultFontSize);
		}

		// Adds a new style or returns existing
		StyleClass&			Add(std::string_view inStyleName, std::string_view inParentStyle = "") {
			auto name = StringID(inStyleName.data());
			auto parentName = StringID(inParentStyle.data());

			const StyleClass* parent = nullptr;

			if(!parentName.Empty()) {
				parent = Find(parentName);
				Assertf(parent, "Cannot find parent style with the name {}", inParentStyle);
			}
			auto [it, bExists] = m_Styles.emplace(name, StyleClass(name, parent));
			return it->second;
		}

		// Merges two themes together
		void				Merge(const Theme* inTheme, bool bOverride = true) {

			for(const auto& [id, style] : inTheme->m_Styles) {
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
		void				RasterizeFonts(std::vector<Font*>* outFonts) {
			// Load fonts
			for(auto& [selector, styleClass] : m_Styles) {

				for(auto& style : styleClass.m_Styles) {

					if(auto* textStyle = style->Cast<TextStyle>()) {

						if(textStyle->Font) {

							if(textStyle->Font->NeedsRebuild()) {
								outFonts->push_back(textStyle->Font);
							}

						} else if(textStyle->Filename.empty()) {
							// If empty set default
							textStyle->Font = m_Fonts.front().get();
							textStyle->Font->RasterizeFace(textStyle->FontSize);
							outFonts->push_back(textStyle->Font);

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
		}

		// Default internal font. Currently ImGui internal
		Font*				GetDefaultFont() { return m_Fonts.front().get(); }

		const StyleClass*	Find(StringID inStyleClass) const {
			auto it = m_Styles.find(inStyleClass);
			Assertf(it != m_Styles.end(), "Style class with the name '{}' not found", inStyleClass.String());
			return it != m_Styles.end() ? &it->second : nullptr;
		}
		
	private:
		std::map<StringID, StyleClass>		m_Styles;
		// All fonts used by this theme
		std::vector<std::unique_ptr<Font>>	m_Fonts;
	};
}