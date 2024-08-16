#pragma once
#include "common.h"

// TODO: hide ft
#include <ft2build.h>
#include FT_FREETYPE_H

#include <expected>


class RenderPass;
struct FT_FaceRec_;

enum class ErrorCode {
    Ok,
    CannotOpenFile,
};


using GlyphID = uint32_t;
using Codepoint = uint32_t;

// Drawing specs of a character used for drawing
struct Glyph {
    uint32_t glyphIndex;
    uint32_t advanceX;
};



// FontTypeface variation axis parameters
// An axis could be a font width, weight, slant, etc.
struct VariationAxis {
    std::string name;
    float min;
    float def;
    float max;
};



// Owns the actual font data
// Uses FreeType to access font data
class FontTypeface: public RefCountedBase {
public:

    struct Metrics {
        uint8_t isMonospace:1;
        uint8_t isBold:1;
        uint8_t isItalic:1;
        uint8_t hasAxes:1;
        uint16_t weight;
        uint16_t width;
        std::string familyName;
        std::string styleName;
        std::vector<VariationAxis> axes;
    };

public:

    static std::expected<RefCountedPtr<FontTypeface>, ErrorCode> Create(
        std::span<char> data);

    FontTypeface() = default;
    ~FontTypeface();

    const std::string& GetFamilyName() const { return metrics_.familyName; }

    // Rasterizes the glyph into a buffer
    bool RasterizeGlyph(GlyphID glyphID, std::vector<char>& outBuffer);

    bool IsVariable() { return metrics_.hasAxes; }

    bool IsBold() { return metrics_.isBold; }

    bool IsItalic() { return metrics_.isItalic; }

    uint32_t GetWeight() { return metrics_.weight; }

    // Unicode 4 bytes codepoint
    // Returns 0 if not found
    GlyphID GlyphForCodepoint(Codepoint codepoint);

    std::span<const char> GetData() const {
        return {(char*)face_->stream->base, face_->stream->size};
    }

private:
    FT_FaceRec_* face_;
    Metrics metrics_;
};


// Light-weight FontTypeface wrapper with specific font parameters set
class Font {
public:
    static std::unique_ptr<Font> Create(RefCountedPtr<FontTypeface> typeface,
                                        uint32_t size) {
        std::unique_ptr<Font> font;
        font->face_ = typeface;
        font->size_ = size;
        return font;
    }

    RefCountedPtr<FontTypeface> GetFaceReferenced() { return face_; }
    FontTypeface* GetFace() { return face_.Get(); }

    uint32_t GetSize() { return size_; }

private:
    RefCountedPtr<FontTypeface> face_;
    uint32_t size_;
};


// Uses FreeType to rasterize glyphs from a font and places them onto
// a 2D texture for faster text rendering
// TODO: Use immediate or defferred context for atlas rendering
class FontAtlas {
public:

    static std::unique_ptr<FontAtlas> Create();

    void AddImage(std::span<char> data);

    // Creates a render pass to render the atlas image on GPU
    std::unique_ptr<RenderPass> CreateRenderPass();

    void InsertGlyphs(std::span<const Glyph> run, Font* font);
};