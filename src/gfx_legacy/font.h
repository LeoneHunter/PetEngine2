#pragma once
#include "base/math_util.h"

#include "image.h"

#include <filesystem>

struct ImFont;
struct ImFontAtlas;
class Image;

namespace ui {

/**
 * Stores font family: normal, bold and italic fonts inside one container.
 *
 * There are 3 implementation variations possible:
 *	- Font atlas which manages all variations of the same font. Variation are
 *accessable through the Font::GetSlice().
 *	- Font atlas which manages all variations of the same font. Variation are
 *accessable directly through the Font::GetParameters().
 *	- Each Font instance per size, bold, italic.
 *
 * For new we stick with the first variant with slices.
 */
class Font {
public:
    static Font* FromInternal();
    static Font* FromFile(const std::filesystem::path& inFilepath);

    /**
     * Font slice parameters needed for rendering
     */
    struct FaceRenderParameters {
        float2 TexUvWhitePixelCoords;
        float* TexUvLinesArrayPtr;
    };

    /**
     * Stores font data for specific font size, bold, italic combination
     */
    class Face {
    public:
        // Containts info about a font Glyph
        // For now ImGuiGlyph
        // Used in rendering
        struct Glyph {
            uint32_t Colored : 1;
            uint32_t Visible : 1;
            uint32_t Codepoint : 30;
            float AdvanceX;
            float X0, Y0, X1, Y1;
            float U0, V0, U1, V1;
        };

        virtual ~Face() = default;

        virtual float GetSize() const = 0;

        virtual float2 CalculateTextSize(const std::string_view& inText,
                                         float inMaxTextWidth,
                                         float inWrapWidth) const = 0;
        virtual float2 CalculateTextSize(const std::wstring_view& inText,
                                         float inMaxTextWidth,
                                         float inWrapWidth) const = 0;

        virtual const char* CalculateWordWrapPosition(
            const std::string_view& inText,
            float inWrapWidth) const = 0;
        virtual const wchar_t* CalculateWordWrapPosition(
            const std::wstring_view& inText,
            float inWrapWidth) const = 0;

        // Rendering parameters for the renderer
        virtual void GetRenderParameters(
            FaceRenderParameters* outParameters) const = 0;
        virtual Glyph GetGlyph(wchar_t inCharacter) const = 0;

        // Get character width in pixels
        virtual float GetGlyphWidth(wchar_t inCharacter) const = 0;

        // Returns parent font
        virtual Font* GetFont() const = 0;
        virtual bool IsBold() const = 0;
        virtual bool IsItalic() const = 0;
    };

    virtual ~Font() = default;

    // Rasterize this font into a new face with different parameters
    virtual bool RasterizeFace(uint8_t inSize,
                               bool bBold = false,
                               bool bItalic = false) = 0;
    virtual bool NeedsRebuild() const = 0;

    // Build internal font data and stores it inside the Image
    virtual bool Build(Image* outImage) = 0;
    virtual void SetAtlasTexture(void* inTextureHandle) = 0;
    virtual void* GetAtlasTexture() const = 0;

    virtual const Face* GetFace(uint8_t inSize,
                                bool bBold = false,
                                bool bItalic = false) const = 0;

    virtual float2 CalculateTextSize(const std::string_view& inText,
                                     uint8_t inFontSize,
                                     bool bBold = false,
                                     bool bItalic = false,
                                     float inMaxTextWidth = FLT_MAX,
                                     float inWrapWidth = 0.f) const = 0;
    virtual float2 CalculateTextSize(const std::wstring_view& inText,
                                     uint8_t inFontSize,
                                     bool bBold = false,
                                     bool bItalic = false,
                                     float inMaxTextWidth = FLT_MAX,
                                     float inWrapWidth = 0.f) const = 0;
};
}  // namespace ui
