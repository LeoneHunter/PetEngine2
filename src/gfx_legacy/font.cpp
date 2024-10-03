#include "font.h"
#include "base/common.h"
#include "base/string_utils.h"

// #define IMGUI_USER_CONFIG "imgui_config.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <filesystem>
#include <map>

using namespace ui;

class ImFontFace final : public Font::Face {
    using CHAR = wchar_t;
    using string_view = std::string_view;
    using wstring_view = std::wstring_view;

public:
    ImFontFace(ImFont* inImFont, Font* inParent, bool bBold, bool bItalic)
        : m_ImFont(inImFont)
        , m_Parent(inParent)
        , m_bBold(bBold)
        , m_bItalic(bItalic) {}

    float GetSize() const override { return m_ImFont->FontSize; }

    float2 CalculateTextSize(const std::string_view& inText,
                             float inMaxTextWidth,
                             float inWrapWidth) const override {
        return m_ImFont->CalcTextSizeA(m_ImFont->FontSize, inMaxTextWidth,
                                       inWrapWidth, inText.data(),
                                       inText.data() + inText.size());
    }

    float2 CalculateTextSize(const std::wstring_view& inText,
                             float inMaxTextWidth,
                             float inWrapWidth) const override {
        float size = m_ImFont->FontSize;
        float max_width = inMaxTextWidth;
        float wrap_width = inWrapWidth;
        const auto* text_begin = inText.data();
        const auto* text_end = text_begin + inText.size();

        // From ImGui
        // Reworked for utf16
        const float line_height = size;
        const float scale = size / m_ImFont->FontSize;

        ImVec2 text_size = ImVec2(0, 0);
        float line_width = 0.0f;

        const bool word_wrap_enabled = (wrap_width > 0.0f);
        const CHAR* word_wrap_eol = NULL;

        const auto* text_char_cursor = text_begin;
        while (text_char_cursor < text_end) {
            if (word_wrap_enabled) {
                // Calculate how far we can render. Requires two passes on the
                // string data but keeps the code simple and not intrusive for
                // what's essentially an uncommon feature.
                if (!word_wrap_eol) {
                    word_wrap_eol = CalculateWordWrapPosition(
                        wstring_view(text_char_cursor, text_end),
                        wrap_width - line_width);
                    if (word_wrap_eol ==
                        text_char_cursor)  // Wrap_width is too small to fit
                                           // anything. Force displaying 1
                                           // character to minimize the height
                                           // discontinuity.
                        word_wrap_eol++;   // +1 may not be a character start
                                           // point in UTF-8 but it's ok because
                                           // we use s >= word_wrap_eol below
                }

                if (text_char_cursor >= word_wrap_eol) {
                    if (text_size.x < line_width)
                        text_size.x = line_width;
                    text_size.y += line_height;
                    line_width = 0.0f;
                    word_wrap_eol = NULL;

                    // Wrapping skips upcoming blanks
                    while (text_char_cursor < text_end) {
                        const auto c = *text_char_cursor;
                        if (ImCharIsBlankW(c)) {
                            text_char_cursor++;
                        } else if (c == '\n') {
                            text_char_cursor++;
                            break;
                        } else {
                            break;
                        }
                    }
                    continue;
                }
            }

            // Decode and advance source
            const auto* prev_s = text_char_cursor;
            const auto text_char = *text_char_cursor;

            text_char_cursor += 1;

            if (text_char < 32) {
                if (text_char == '\n') {
                    text_size.x = std::max(text_size.x, line_width);
                    text_size.y += line_height;
                    line_width = 0.0f;
                    continue;
                }
                if (text_char == '\r')
                    continue;
            }

            const float char_width =
                ((int)text_char < m_ImFont->IndexAdvanceX.Size
                     ? m_ImFont->IndexAdvanceX.Data[text_char]
                     : m_ImFont->FallbackAdvanceX) *
                scale;
            if (line_width + char_width >= max_width) {
                text_char_cursor = prev_s;
                break;
            }

            line_width += char_width;
        }

        if (text_size.x < line_width)
            text_size.x = line_width;

        if (line_width > 0 || text_size.y == 0.0f)
            text_size.y += line_height;

        return text_size;
    }

    const char* CalculateWordWrapPosition(const std::string_view& inText,
                                          float inWrapWidth) const override {
        return m_ImFont->CalcWordWrapPositionA(
            1.f, inText.data(), inText.data() + inText.size(), inWrapWidth);
    }

    // From ImGui::ImFont
    const wchar_t* CalculateWordWrapPosition(const std::wstring_view& inText,
                                             float wrap_width) const override {
        float line_width = 0.0f;
        float word_width = 0.0f;
        float blank_width = 0.0f;

        const auto* word_end = inText.data();
        const CHAR* prev_word_end = NULL;
        bool inside_word = true;

        const auto* s = inText.data();

        while (s < inText.data() + inText.size()) {
            auto c = *s;
            const CHAR* next_s;

            next_s = s + 1;

            if (c < 32) {
                if (c == '\n') {
                    line_width = word_width = blank_width = 0.0f;
                    inside_word = true;
                    s = next_s;
                    continue;
                }
                if (c == '\r') {
                    s = next_s;
                    continue;
                }
            }

            const float char_width = ((int)c < m_ImFont->IndexAdvanceX.Size
                                          ? m_ImFont->IndexAdvanceX.Data[c]
                                          : m_ImFont->FallbackAdvanceX);
            if (ImCharIsBlankW((uint32_t)c)) {
                if (inside_word) {
                    line_width += blank_width;
                    blank_width = 0.0f;
                    word_end = s;
                }
                blank_width += char_width;
                inside_word = false;
            } else {
                word_width += char_width;
                if (inside_word) {
                    word_end = next_s;
                } else {
                    prev_word_end = word_end;
                    line_width += word_width + blank_width;
                    word_width = blank_width = 0.0f;
                }

                // Allow wrapping after punctuation.
                inside_word = (c != '.' && c != ',' && c != ';' && c != '!' &&
                               c != '?' && c != '\"');
            }
            // We ignore blank width at the end of the line (they can be
            // skipped)
            if (line_width + word_width > wrap_width) {
                // Words that cannot possibly fit within an entire line will be
                // cut anywhere.
                if (word_width < wrap_width)
                    s = prev_word_end ? prev_word_end : word_end;
                break;
            }
            s = next_s;
        }
        return s;
    }

    void GetRenderParameters(Font::FaceRenderParameters* outParameters) const {
        DASSERT(outParameters);
        outParameters->TexUvLinesArrayPtr =
            (float*)m_ImFont->ContainerAtlas->TexUvLines;
        outParameters->TexUvWhitePixelCoords =
            m_ImFont->ContainerAtlas->TexUvWhitePixel;
    }

    Glyph GetGlyph(wchar_t inCharacter) const override {
        auto* imGlyph = m_ImFont->FindGlyph((ImWchar)inCharacter);
        return {
            imGlyph->Colored,  imGlyph->Visible, imGlyph->Codepoint,
            imGlyph->AdvanceX, imGlyph->X0,      imGlyph->Y0,
            imGlyph->X1,       imGlyph->Y1,      imGlyph->U0,
            imGlyph->V0,       imGlyph->U1,      imGlyph->V1,
        };
    }

    float GetGlyphWidth(wchar_t inCharacter) const override {
        return m_ImFont->GetCharAdvance(inCharacter);
    }

    Font* GetFont() const override { return m_Parent; }

    bool IsBold() const override { return m_bBold; }

    bool IsItalic() const override { return m_bItalic; }

private:
    ImFont* m_ImFont;
    Font* m_Parent;
    bool m_bBold;
    bool m_bItalic;
};

/**
 * Key used for quick search
 */
struct FontKey {
    FontKey(uint8_t inSize, bool bBold, bool bItalic)
        : size(inSize), bBold(bBold), bItalic(bItalic), _unused(0) {}

    union {
        struct {
            uint16_t size : 8;
            uint16_t bBold : 1;
            uint16_t bItalic : 1;
            uint16_t _unused : 6;
        };
        uint16_t key;
    };
};

bool operator==(const FontKey& left, const FontKey& right) {
    return left.key == right.key;
}
bool operator!=(const FontKey& left, const FontKey& right) {
    return left.key != right.key;
}
bool operator<(const FontKey& left, const FontKey& right) {
    return left.key < right.key;
}
bool operator>(const FontKey& left, const FontKey& right) {
    return left.key > right.key;
}

namespace fs = std::filesystem;

/**
 * Wrapper around ImGui::ImFont
 * Maybe move to FreeType in future
 */
class ImGuiFont final : public Font {
public:
    ImGuiFont(const std::filesystem::path& inFilepath)
        : m_Filepath(inFilepath), m_Flags(Flags::None) {
        m_FontAtlas.Flags |= ImFontAtlasFlags_NoMouseCursors;

        auto filepath = inFilepath;
        const auto fontName = filepath.stem().string();

        // bold
        std::string nameSuffix = "B";
        filepath = m_Filepath.parent_path() /
                   (m_Filepath.stem().string() + nameSuffix);
        filepath.replace_extension(m_Filepath.extension());

        if (fs::exists(filepath)) {
            m_Flags |= Flags::BoldAvailable;
        } else {
            LOGF(Verbose, "Info: Bold style for the font {} is not available.",
                 fontName);
        }

        // italic
        nameSuffix = "I";
        filepath = m_Filepath.parent_path() /
                   (m_Filepath.stem().string() + nameSuffix);
        filepath.replace_extension(m_Filepath.extension());

        if (fs::exists(filepath)) {
            m_Flags |= Flags::ItalicAvailable;
        } else {
            LOGF(Verbose,
                 "Info: Italic style for the font {} is not available.",
                 fontName);
        }

        // both
        nameSuffix = "BI";
        filepath = m_Filepath.parent_path() /
                   (m_Filepath.stem().string() + nameSuffix);
        filepath.replace_extension(m_Filepath.extension());

        if (fs::exists(filepath)) {
            m_Flags |= Flags::BoldItalicAvailable;
        } else {
            LOGF(Verbose,
                 "Info: Bold Italic style for the font {} is not available.",
                 fontName);
        }
    }

    bool RasterizeFace(uint8_t inSize,
                       bool bBold = false,
                       bool bItalic = false) override {
        const auto key = FontKey(inSize, bBold, bItalic);

        // Check if it's already pending
        auto it = m_PendingFaces.find(key);

        if (it != m_PendingFaces.end()) {
            return true;
        }

        // Check if it's already loaded
        auto it2 = m_Faces.find(key);

        if (it2 != m_Faces.end()) {
            return true;
        }

        ImFont* imFont = nullptr;
        ImFontConfig config;
        config.SizePixels = inSize;

        if (m_Filepath.empty()) {
            config.OversampleH = 1;
            config.OversampleV = 1;
            config.PixelSnapH = true;
            imFont = m_FontAtlas.AddFontDefault(&config);

        } else {
            auto filepath = m_Filepath;
            std::string nameSuffix;
            const auto fontName = m_Filepath.stem().string();
            bool bStyleAvailable = true;

            if (bBold && bItalic) {
                if (m_Flags & Flags::BoldItalicAvailable) {
                    nameSuffix = "BI";
                } else {
                    LOGF(Verbose,
                         "Error: Cannot rasterize the font {} with the Bold "
                         "Italic style!\nFalling back to the normal font.",
                         fontName);
                    bStyleAvailable = false;
                }

            } else if (bBold) {
                if (m_Flags & Flags::BoldAvailable) {
                    nameSuffix = "B";
                } else {
                    LOGF(Verbose,
                         "Error: Cannot rasterize the font {} with the Bold "
                         "Italic style!\nFalling back to the normal font.",
                         fontName);
                    bStyleAvailable = false;
                }

            } else if (bItalic) {
                if (m_Flags & Flags::ItalicAvailable) {
                    nameSuffix = "I";
                } else {
                    LOGF(Verbose,
                         "Error: Cannot rasterize the font {} with the Bold "
                         "Italic style!\nFalling back to the normal font.",
                         fontName);
                    bStyleAvailable = false;
                }
            }

            // If style is not available - rasterize normal style of the same
            // size
            if (!bStyleAvailable) {
                auto* rasterizedNormalSlice = FindFace(inSize, false, false);

                if (!rasterizedNormalSlice) {
                    RasterizeFace(inSize, false, false);
                }
                return true;
            }

            if (!nameSuffix.empty()) {
                filepath = m_Filepath.parent_path() /
                           (m_Filepath.stem().string() + nameSuffix);
                filepath.replace_extension(m_Filepath.extension());
            }

            if (!fs::exists(filepath)) {
                LOGF(Verbose, "Error: Cannot load font '{}'. File not found!",
                     filepath);
                return false;
            }

            // Full range
            // static const ImWchar ranges[] = {0x0020, 0xFFFD, 0};

            static const ImWchar ranges[] = {
                0x0020,
                0x00FF,  // Basic Latin + Latin Supplement
                0x0400,
                0x052F,  // Cyrillic + Cyrillic Supplement
                0x2DE0,
                0x2DFF,  // Cyrillic Extended-A
                0xA640,
                0xA69F,  // Cyrillic Extended-B
                0,
            };

            config.OversampleH = 3;
            config.OversampleV = 3;
            config.PixelSnapH = true;
            config.GlyphRanges = ranges;

            imFont = m_FontAtlas.AddFontFromFileTTF(filepath.string().c_str(),
                                                    inSize, &config);
        }

        m_PendingFaces.emplace(key, imFont);
        return true;
    }

    bool NeedsRebuild() const override { return !m_PendingFaces.empty(); }

    // Build internal font data and stores it inside the Image
    bool Build(Image* outImage) override {
        m_FontAtlas.Build();
        outImage->m_Format = Image::Format::RGBA;

        unsigned char* pixels;
        int width, height;
        m_FontAtlas.GetTexDataAsRGBA32(&pixels, &width, &height);
        const auto dataSize = (size_t)width * height * 4;

        outImage->Height = height;
        outImage->Width = width;
        outImage->Data.resize(dataSize);
        memcpy(outImage->Data.data(), pixels, dataSize);

        // Add faces to the list
        for (auto [key, pendingFace] : m_PendingFaces) {
            m_Faces.emplace(
                key, new ImFontFace(pendingFace, this, key.bBold, key.bItalic));
        }
        m_PendingFaces.clear();

        return true;
    }

    void SetAtlasTexture(void* inTextureHandle) override {
        m_FontAtlas.SetTexID(inTextureHandle);
    }

    void* GetAtlasTexture() const override { return m_FontAtlas.TexID; }

    const Face* GetFace(uint8_t inSize,
                        bool bBold = false,
                        bool bItalic = false) const override {
        const auto key = FontKey(inSize, bBold, bItalic);
        auto it = m_Faces.find(key);

        if (it != m_Faces.end()) {
            return it->second;
        }

        bool bStyleAvailable = false;

        if (bBold && bItalic) {
            if (m_Flags & Flags::BoldItalicAvailable) {
                bStyleAvailable = true;
            }
        } else if (bBold) {
            if (m_Flags & Flags::BoldAvailable) {
                bStyleAvailable = true;
            }
        } else if (bItalic) {
            if (m_Flags & Flags::ItalicAvailable) {
                bStyleAvailable = true;
            }
        }

        // If such style is not available fall back to the normal face of the
        // same size
        if (!bStyleAvailable) {
            auto it2 = m_Faces.find(FontKey(inSize, false, false));
            if (it2 != m_Faces.end()) {
                return it->second;
            }
        }
        return nullptr;
    }

    float2 CalculateTextSize(const std::string_view& inText,
                             uint8_t inFontSize,
                             bool bBold,
                             bool bItalic,
                             float inMaxTextWidth,
                             float inWrapWidth) const override {
        auto* face = GetFace(inFontSize, bBold, bItalic);
        if (!face) {
            return {};
        }
        return face->CalculateTextSize(inText, inMaxTextWidth, inWrapWidth);
    }

    float2 CalculateTextSize(const std::wstring_view& inText,
                             uint8_t inFontSize,
                             bool bBold,
                             bool bItalic,
                             float inMaxTextWidth,
                             float inWrapWidth) const override {
        auto* face = GetFace(inFontSize, bBold, bItalic);
        if (!face) {
            return {};
        }
        return face->CalculateTextSize(inText, inMaxTextWidth, inWrapWidth);
    }

private:
    ImFontFace* FindFace(uint8_t inSize,
                         bool bBold = false,
                         bool bItalic = false) {
        const auto key = FontKey(inSize, bBold, bItalic);
        auto it = m_Faces.find(key);

        if (it != m_Faces.end()) {
            return it->second;
        }
        return nullptr;
    }

    const ImFontFace* FindFace(uint8_t inSize,
                               bool bBold = false,
                               bool bItalic = false) const {
        const auto key = FontKey(inSize, bBold, bItalic);
        auto it = m_Faces.find(key);

        if (it != m_Faces.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    enum class Flags {
        None = 1 << 0,
        BoldAvailable =
            1 << 1,  // File of this font with the Bold style is available
        ItalicAvailable =
            1 << 2,  // File of this font with the Italic style is available
        BoldItalicAvailable =
            1 << 3  // File of this font with the Bold Italic style is available
    };

    DEFINE_ENUM_FLAGS_OPERATORS_FRIEND(Flags)

private:
    std::filesystem::path m_Filepath;
    Flags m_Flags;
    // Contains image with all glyphs used for rendering
    ImFontAtlas m_FontAtlas;
    // Array of fonts with different sizes and formats
    // Atlas owns font all variants
    std::map<FontKey, ImFontFace*> m_Faces;

    // Slices that should be builded
    // We cannot build on the fly because font texture could be used
    std::map<FontKey, ImFont*> m_PendingFaces;
};

Font* ui::Font::FromInternal() {
    return new ImGuiFont("");
}

Font* ui::Font::FromFile(const std::filesystem::path& inFilepath) {
    return new ImGuiFont(inFilepath);
}