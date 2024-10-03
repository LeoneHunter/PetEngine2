#include "base/common.h"
#include "base/util.h"
#include "gfx/common.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "thirdparty/utf8-decoder/utf8-decoder.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ot.h>

#include <filesystem>
#include <fstream>
#include <ranges>
#include <mdspan>
#include <iostream>

#include <Windows.h>


template <class... Types>
void PrintError(const std::format_string<Types...> fmt, Types&&... args) {
    Println("Error: {}", std::format(fmt, std::forward<Types>(args)...));
}

uint32_t DecodeUtf8(const std::string& str) {
    Utf8DfaDecoder::State state = Utf8DfaDecoder::kAccept;
    uint32_t out = 0;
    for(int i = 0; i < 4 && i < str.size(); ++i) {
        const uint8_t c = str[i];
        Utf8DfaDecoder::Decode(c, &state, &out);
        switch(state) {
            case Utf8DfaDecoder::kAccept: return out;
            case Utf8DfaDecoder::kReject: return 0;
        }
    }
    return 0;
}

// TODO: Consider multithreading
// Bind with harfbuzz
// Wrap in RAII
// Different fonts for different ranges (specific font for icons, cjk, cyrillic)
// Fonts stack and fallback fonts
// A text should be split into segments and each segment can be totally independent (font, features, script)
// Variable fonts
// 
// No support for:
// - Most of OpenType features
// - Font synthesis
// - Variant caps and other style variations
// - Vertical and horizontal text mixing
// - Unicode case conversion
// - Font stretch

// Basic Freetype test
int main(int argc, char* argv[]) {
    std::cout << GetCommandLineA() << std::endl;

    CommandLine::Set(argc, argv);
    if(CommandLine::ArgSize() == 0) {
        PrintError("No argumets are provided");
        Println("Usage: freetype_example -F C:/Windows/Fonts/SomeFont.ttf -U8 F\
        Will print the character 'F' in ascii form from the font.");
        return 0;
    }
    std::string filename = CommandLine::ParseArgOr<std::string>("-F", "");
    if(filename.empty()) {
        PrintError("Font filename is empty");
        return 0;
    }
    const std::string u8char = CommandLine::ParseArgOr<std::string>("-U8", "");
    if(u8char == "") {
        PrintError("Invalid character");
        return 0;
    }
    for(char c: u8char) {
        Println("0x{:04x}", (uint8_t)c);
    }
    const uint32_t codepoint = DecodeUtf8(u8char);
    if(codepoint == 0) {
        PrintError("Invalid character");
        return 0;
    }

    // Start loading font
    std::error_code ec;
    auto fileSize = std::filesystem::file_size(filename, ec);
    DASSERT_F(!ec, "Cannot find the file: {}", filename);

    std::vector<char> fileBlob;
    fileBlob.resize(fileSize);

    std::ifstream fontFile;
    fontFile.open(filename, std::ios::binary);
    DASSERT(fontFile.good());

    fontFile.read(fileBlob.data(), fileSize);
    DASSERT(fontFile.good());
    DASSERT(fontFile.gcount() == fileSize);
    fontFile.close();

    std::string font_family_name;

    // Basic test
    FT_Library library;
    FT_Init_FreeType(&library);
    FT_Face face;
    FT_Open_Args open_args = {
        FT_OPEN_MEMORY,
        reinterpret_cast<FT_Byte*>(fileBlob.data()),
        static_cast<FT_Long>(fileSize)
    };
    constexpr auto kFaceIndex = 0;
    auto err = FT_Open_Face(library, &open_args, kFaceIndex, &face);
    DASSERT(err == FT_Err_Ok);
    font_family_name = FT_Get_Postscript_Name(face);
    Println("Loaded the font: {}", font_family_name);

    // Load and rasterize 'M' glyph
    constexpr auto kFontSize = 20;
    err = FT_Set_Pixel_Sizes(face, 0, kFontSize);
    DASSERT(err == FT_Err_Ok);

    // Uses UTF-32, basically a unicode codepoint
    // First lets try a ASCII char
    // The glyph index returned could be a "missing glyph" if it's not present
    // in the font
    // TODO: glyphIndex will be 0 if there's no glyph for that character, so use
    // anorher font
    uint32_t emoji = 0x1f600;
    uint32_t cyrillic = 0x0416;
    uint32_t ch = codepoint;
    auto glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(ch));

    err = FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_COLOR);
    DASSERT(err == FT_Err_Ok);
    auto& glyph = face->glyph;

    std::string glyphFormat = 
        glyph->format == FT_GLYPH_FORMAT_OUTLINE ? "OUTLINE" :
        glyph->format == FT_GLYPH_FORMAT_BITMAP ? "BITMAP" : "UNKNOWN";

    if(glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        // 8-bit (1 char) per pixel
        err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD);
        DASSERT(err == FT_Err_Ok);
    }

    auto& bitmap = glyph->bitmap;
    Println("Character: {:#06x}", (uint32_t)ch);
    Println("Glyph index: {}", glyphIndex);
    Println("Glyph format: {}", glyphFormat);
    Println("Bitmap format: {}", 
        bitmap.pixel_mode == FT_PIXEL_MODE_GRAY || bitmap.pixel_mode == FT_PIXEL_MODE_LCD ? "GRAYSCALE" : 
        bitmap.pixel_mode == FT_PIXEL_MODE_BGRA ? "COLORED" : "UNKNOWN");
    Println("Width: {}", bitmap.width);
    Println("Pitch: {}", bitmap.pitch);
    Println("Height: {}", bitmap.rows);
    Println("=== Bitmap ===");

    std::string buf;
    buf.reserve(bitmap.pitch);

    auto printGray = [&](std::span<uint8_t> row) {
        for(auto pixel: row) {
            if(pixel < 10) {
                buf.push_back(' ');
            } else if(pixel < 100) {
                buf.push_back('.');
            } else if(pixel < 200) {
                buf.push_back(':');
            } else {
                buf.push_back('#');
            }
        }
    };

    auto printColored = [&](std::span<uint8_t> row) {
        auto f = row | std::views::chunk(4);
        for(auto pixelBGRA: row | std::views::chunk(4)) {
            auto pixel = pixelBGRA[2];
            if(pixel < 10) {
                buf.push_back(' ');
            } else if(pixel < 100) {
                buf.push_back('.');
            } else if(pixel < 200) {
                buf.push_back(':');
            } else {
                buf.push_back('#');
            }
        }
    };

    for(size_t row = 0; row < bitmap.rows; ++row) {
        auto rowOffset = bitmap.buffer + (row * bitmap.pitch);
        if(bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
            printColored(std::span(rowOffset, bitmap.pitch));
        } else {
            printGray(std::span(rowOffset, bitmap.pitch));
        }
        Println("{}", buf);
        buf.clear();
    }
    Println("==============");

    // Cleanup
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return 0;
}

