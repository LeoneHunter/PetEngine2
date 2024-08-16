#include "common.h"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_internal.h"

#include "v1.h"

#include "font.h"
#include "shaper.h"
#include "font_cache.h"
#include "canvas.h"

// FreeType library
// #include <ft2build.h>
// #include FT_FREETYPE_H

// HarfBuzz shaper
#include <hb-ft.h>
#include <hb-ot.h>
#include <hb.h>

#include <expected>
#include <stack>

// Orientation of the whole text
enum class TextOrientation {
    Normal,
    Right,
    Left,
};

// Direction of writing
enum class TextDirection {
    LeftToRight,
    RightToLeft,
    TopToBottom
};

enum class ErrorCode {
    Ok,
    CannotOpenFile,
};


// A set of parameters describing the visual style of a text
class FontStyle {
public:

    enum class FallbackFont {
        Internal,
        Serif,
        SansSerif,
        Monospace,
        Cursive,
    };

    enum class Style {
        Normal,
        Italic,
    };

    enum class Weight {
        Thin = 200,
        Normal = 500,
        Bold = 800,
    };

public:
    FontStyle() 
        : fallback(FallbackFont::Internal)
        , weight(Weight::Normal)
        , style(Style::Normal)
        , size(13)
    {}

    FontStyle& SetFamilyStack(
        std::convertible_to<std::string> auto... families) {
        (families.push_back(families), ...);
        return *this;
    }

    FontStyle& SetFallback(FallbackFont fallback) {
        this->fallback = fallback;
        return *this;
    }

    FontStyle& SetWeight(Weight weight) {
        this->weight = weight;
        return *this;
    }

    FontStyle& SetSize(uint32_t size) {
        this->size = size;
        return *this;
    }

    FontStyle& SetStyle(Style style) {
        this->style = style;
        return *this;
    }

public:
    std::vector<std::string> families;
    FallbackFont fallback;
    Weight weight;
    Style style;
    uint32_t size;
};


// Complete set of text style parameters
// Some are used in shaping
// Some are used in drawing
struct TextStyle {
    FontStyle fontStyle;
    gfx::Color4f color;
    gfx::Color4f shadowColor;
    gfx::Vec2f shadowOffset;
    TextOrientation orientation;
    TextDirection direction;
    bool shadow;
    bool strikeThrough;
    bool underline;

    static TextStyle Default() {
        return {
            .fontStyle = FontStyle{},
            .orientation = TextOrientation::Normal,
            .direction = TextDirection::LeftToRight,
            .shadow = false,
            .strikeThrough = false,
            .underline = false,
        };
    }
};





class GlyphInfo {
    gfx::Rect texcoords;
    uint32_t advanceX;
    uint32_t advanceY;
};

// Caches rasterized font glyphs
// TODO: Use a single cache for all fonts or a cache per font instance
class GlyphCache {
public:

    std::optional<GlyphInfo> FindGlyph(uint32_t codepoint);
    std::optional<GlyphInfo> FindOrAddGlyph(uint32_t codepoint);

private:
    RefCountedPtr<rendering::Texture> atlas_;
};


void CreateFont() {
    // Generic font face. From raw font file.
    // Owns the actual font data (GPOS, GSUB tables, glyphs)
    auto face = HBFaceUniquePtr(hb_face_create(nullptr, 0));
    auto font = HBFontUniquePtr(hb_font_create(face.get()));
    // hb_font_set_ppem(font, 0, 13);
}


class Texture;


void Test() {
    const auto text = std::string("Hello world!");
    const auto color = gfx::Color4f::FromHexCode("#454545");
    const auto fontStyle = FontStyle()
        .SetFamilyStack("Arial", "Roboto")
        .SetSize(13)
        .SetFallback(FontStyle::FallbackFont::Internal)
        .SetStyle(FontStyle::Style::Normal)
        .SetWeight(FontStyle::Weight::Normal);

    // Prepare fonts
    // - Load fonts
    // - Add loaded fonts into a FontCache
    // Shaping
    // - Segment the text
    // - Create a shaper with a FontCache and GlyphCache
    // - Match a font from a family list
    // - Try to shape the text with the font. Get a GlyphRun of the text
    std::unique_ptr<FontCache> fontCache = FontCache::Create(64);
    fontCache->LoadAllFromDirectory("res/fonts");
    RefCountedPtr<FontTypeface> face = fontCache->Match(
        fontStyle.families, (uint32_t)fontStyle.weight, false);
    std::unique_ptr<Font> font = Font::Create(face, 13);

    std::unique_ptr<TextShaper> shaper = TextShaper::Create();
    GlyphRun glyphRun = shaper->Shape(text, font.get());

    // Prepare drawing
    // - Create a FontAtlas
    // - Rasterize glyphs the glyph cache is caching
    // - Upload the atlas texture into GPU and store the texture for future
    std::unique_ptr<FontAtlas> atlas = FontAtlas::Create();
    atlas->InsertGlyphs(glyphRun, font.get());

    std::unique_ptr<RenderPass> atlasRenderPass = atlas->CreateRenderPass();
    // Submit the pass to the render context for actual rendering
    RefCountedPtr<Texture> atlasTexture;

    // Drawing
    // - Create a Canvas
    // - Draw a GlyphRun on the canvas using the texture from the atlas
    std::unique_ptr<gfx::Canvas> canvas = gfx::Canvas::Create();
    canvas->DrawFill(gfx::colors::kWhite);
    canvas->DrawGlyphRun(glyphRun, *atlasTexture);

    std::unique_ptr<RenderPass> canvasRenderPass = canvas->CreateRenderPass();
    RefCountedPtr<Texture> canvasTexture;
}