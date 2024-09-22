#pragma once
#include "common.h"
#include "font.h"

// TODO: hide
#include <hb-ft.h>
#include <hb-ot.h>
#include <hb.h>

using HBBlobUniquePtr = std::unique_ptr<hb_blob_t, Deleter<hb_blob_destroy>>;
using HBFaceUniquePtr = std::unique_ptr<hb_face_t, Deleter<hb_face_destroy>>;
using HBFontUniquePtr = std::unique_ptr<hb_font_t, Deleter<hb_font_destroy>>;
using HBBufferUniquePtr =
    std::unique_ptr<hb_buffer_t, Deleter<hb_buffer_destroy>>;

// Orientation of the whole text
enum class TextOrientation {
    Normal,
    Right,
    Left,
};

// Direction of writing
enum class TextDirection { LeftToRight, RightToLeft, TopToBottom };

// Parameters of a font
struct FontStyle {
    uint32_t size;
    bool italic;
    bool bold;

    static constexpr uint32_t kDefaultFontSize = 13;

    static FontStyle Default(uint32_t size = kDefaultFontSize) {
        return {.size = size, .italic = false, .bold = false};
    }
};


// Complete set of text style parameters
// Some are used in shaping
// Some are used in drawing
struct TextStyle {
    FontStyle fontStyle;
    gfx::Color4f color;
    // TODO: Make as enum
    uint32_t spacing;
    gfx::Color4f shadowColor;
    gfx::Vec2f shadowOffset;
    TextOrientation orientation;
    TextDirection direction;
    bool shadow;
    bool strikeThrough;
    bool underline;

    static TextStyle Default() {
        return {
            .fontStyle = FontStyle{.size = 13, .italic = false, .bold = false},
            .spacing = 10,
            .orientation = TextOrientation::Normal,
            .direction = TextDirection::LeftToRight,
            .shadow = false,
            .strikeThrough = false,
            .underline = false,
        };
    }
};



// Stores a text string with specific visual parameters
// like style, shadow, font stack
// These parameters are applied to the whole run, so complex texts should
// be split into parts first
class GlyphRun {
public:
    GlyphRun() = default;
    GlyphRun(const std::string& text, const FontStyle& style);

    void Reserve(size_t size);

    void Clear();

    size_t Size() const;

    // Pushes a new glyph at the end
    Glyph& EmplaceBack();

    gfx::Vec2f GetBoundingBox() const;

    // Read only iteration
    auto begin() const { return glyphs_.begin(); }
    auto end() const { return glyphs_.end(); }

private:
    std::vector<Glyph> glyphs_;
    std::unique_ptr<Font> font_;
};


// Shapes a text using the HarfBuzz
// I.e. scans the text, converts characters into glyphs and appends them to a
// list ready for rendering or measuring
// TODO:
// - Add glyph cache
// - Run shaping of large texts in parallel
// - Detect language and script from the text
// - Split the initial text into runs based on script and language
// - Try to reshape invalid characters from the input
class TextShaper {
public:
    static std::unique_ptr<TextShaper> Create();

    // Shapes the utf8 encoded |text| using the font |font|
    GlyphRun Shape(std::string_view text, Font* font);

    // Because of caches
    TextShaper(const TextShaper&) = delete;
    TextShaper& operator==(const TextShaper&) = delete;

private:
    hb_script_t HbScriptFromCodepoint(uint32_t codepoint) {
        return hb_unicode_script(hb_unicode_funcs_get_default(), codepoint);
    }

    hb_face_t* CacheFindFace(FontTypeface* face);

private:
    constexpr static int kFaceCacheSize = 32;

    HBBufferUniquePtr buffer_;
    // We create and cache a hb_face_t for every used face
    // We use simple LRU here
    std::vector<HBFaceUniquePtr> faceCache_;
};