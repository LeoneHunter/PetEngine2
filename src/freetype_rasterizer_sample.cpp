#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

// libpng
#include <png.h>

#undef max
#undef min

using uint32 = uint32_t;
using int32 = int32_t;
using uint8 = uint8_t;


constexpr uint32 kAtlasWidth = 512;
constexpr uint32 kAtlasHeight = 512;
constexpr uint32 kFirstChar = 32;
constexpr uint32 kLastChar = 126;


struct Glyph {
  char character;
  int32 width;
  int32 height;
};


std::vector<Glyph> LoadGlyphs(FT_Face face) {
  std::vector<Glyph> glyphs;
  glyphs.reserve(95);  // For printable ASCII 32-126

  for (char c = 32; c <= 126; ++c) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      std::cerr << "Failed to load character: " << c << '\n';
      continue;
    }
    Glyph glyph{.character = c,
                .width = static_cast<int32_t>(face->glyph->bitmap.width),
                .height = static_cast<int32_t>(face->glyph->bitmap.rows)};
    glyphs.push_back(glyph);
  }
  return glyphs;
}

struct PackedGlyph {
  int32 x, y;
  int32 width, height;
};

struct AtlasPackingResult {
  int32 atlasWidth;
  int32 atlasHeight;
  std::unordered_map<char, PackedGlyph> placements;
};

// Naive bin-packing into R rows with best heuristic
AtlasPackingResult PackGlyphs(const std::vector<Glyph>& glyphs,
                              uint32 rowCount) {
  const int32 padding = 1;
  AtlasPackingResult out;

  std::vector<std::vector<Glyph>> rows(rowCount);

  // Greedy row fill: tallest glyphs first
  auto sorted = glyphs;
  std::sort(sorted.begin(), sorted.end(),
            [](const Glyph& a, const Glyph& b) { return a.height > b.height; });

  for (size_t i = 0; i < sorted.size(); ++i) {
    rows[i % rowCount].push_back(sorted[i]);
  }

  int32 atlasWidth = 0;
  int32 atlasHeight = 0;
  std::unordered_map<char, PackedGlyph> placements;

  int32 y = 0;
  for (const auto& row : rows) {
    int32 rowHeight = 0;
    int32 x = 0;
    for (const auto& glyph : row) {
      placements[glyph.character] = {x, y, glyph.width, glyph.height};
      x += glyph.width + padding;
      rowHeight = std::max(rowHeight, glyph.height);
    }
    atlasWidth = std::max(atlasWidth, x);
    y += rowHeight + padding;
  }
  atlasHeight = y;

  out.atlasWidth = atlasWidth;
  out.atlasHeight = atlasHeight;
  out.placements = std::move(placements);
  return out;
}


struct GlyphInfo {
  // texture coordinates
  float u1, v1, u2, v2;
  int32 width, height;
  int32 bearingX, bearingY;
  int32 advance;
};

void Rasterize(FT_Face face,
               std::vector<uint8_t>& atlas,
               uint32 atlasWidth,
               uint32 atlasHeight,
               std::unordered_map<char, GlyphInfo>& glyphMap,
               const std::unordered_map<char, PackedGlyph>& placements) {
  if (!face || !face->size) {
    throw std::runtime_error("Invalid FreeType face.");
  }

  // Assume grayscale 8-bit format (1 channel)
  for (const auto& [ch, placement] : placements) {
    if (FT_Load_Char(face, ch, FT_LOAD_RENDER)) {
      throw std::runtime_error(std::string("Failed to load character: ") + ch);
    }

    FT_GlyphSlot g = face->glyph;
    if (g->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
      throw std::runtime_error("Only grayscale fonts are supported.");
    }

    // Safety check
    if (placement.x + g->bitmap.width > atlasWidth ||
        placement.y + g->bitmap.rows > atlasHeight) {
      throw std::runtime_error("Glyph placement out of atlas bounds.");
    }

    // Copy glyph bitmap into atlas at (x, y)
    for (uint32 row = 0; row < g->bitmap.rows; ++row) {
      std::memcpy(&atlas[(placement.y + row) * atlasWidth + placement.x],
                  &g->bitmap.buffer[row * g->bitmap.pitch], g->bitmap.width);
    }

    // Fill GlyphInfo
    GlyphInfo info;
    info.u1 = static_cast<float>(placement.x) / atlasWidth;
    info.v1 = static_cast<float>(placement.y) / atlasHeight;
    info.u2 = static_cast<float>(placement.x + placement.width) / atlasWidth;
    info.v2 = static_cast<float>(placement.y + placement.height) / atlasHeight;

    info.width = static_cast<int32_t>(g->bitmap.width);
    info.height = static_cast<int32_t>(g->bitmap.rows);
    info.bearingX = g->bitmap_left;
    info.bearingY = g->bitmap_top;
    info.advance =
      static_cast<int32_t>(g->advance.x >> 6);  // Convert from 26.6 fixed-point

    glyphMap[ch] = info;
  }
}

bool SaveAtlasAsPNG(const char* filename,
                    const std::vector<uint8_t>& atlasData,
                    int atlasWidth,
                    int atlasHeight) {
  FILE* fp = fopen(filename, "wb");
  if (!fp) {
    perror("fopen");
    return false;
  }

  png_structp png =
    png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    fclose(fp);
    return false;
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, nullptr);
    fclose(fp);
    return false;
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return false;
  }

  png_init_io(png, fp);

  png_set_IHDR(png, info, atlasWidth, atlasHeight, 8, PNG_COLOR_TYPE_GRAY,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);

  png_write_info(png, info);

  std::vector<png_bytep> row_pointers(atlasHeight);
  for (int y = 0; y < atlasHeight; ++y) {
    row_pointers[y] = (png_bytep)&atlasData[y * atlasWidth];
  }

  png_write_image(png, row_pointers.data());
  png_write_end(png, nullptr);

  png_destroy_write_struct(&png, &info);
  fclose(fp);
  return true;
}

bool CreateFontAtlas(const std::filesystem::path& path,
                     uint32 size,
                     const std::filesystem::path& output) {
  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    std::cerr << "Failed to init FreeType\n";
    return false;
  }

  FT_Face face;
  if (FT_New_Face(ft, path.string().c_str(), 0, &face)) {
    std::cerr << "Failed to load font\n";
    return false;
  }

  FT_Set_Pixel_Sizes(face, 0, size);
  std::vector<Glyph> glyphSizes = LoadGlyphs(face);
  AtlasPackingResult packing = PackGlyphs(glyphSizes, 12);

  auto atlas = std::vector<uint8>(kAtlasWidth * kAtlasHeight, 0);
  auto glyphMap = std::unordered_map<char, GlyphInfo>();
  Rasterize(face, atlas, packing.atlasWidth, packing.atlasHeight, glyphMap,
            packing.placements);
  SaveAtlasAsPNG(output.string().c_str(), atlas, packing.atlasWidth,
                 packing.atlasHeight);

  FT_Done_Face(face);
  FT_Done_FreeType(ft);
  return true;
}


int main(int argv, const char* argc[]) {
  if (argv < 4) {
    std::cout << "Please provide: font_filename font_size output_name\n";
    return 1;
  }
  auto input = std::filesystem::path(argc[0]).parent_path() / argc[1];
  auto output = std::filesystem::path(argc[0]).parent_path() / argc[3];
  CreateFontAtlas(input, atoi(argc[2]), output);
  return 0;
}






















// CREATE_FONT_ATLAS: 
//  Initialize Freetype
//  Get metrics for latin script
//  Pack glyphs into a 2D space
//  Rasterize packed glyphs into the buffer
//    Calculate texture coordinates
//  Initialize rendering framework
//  Create an atlas texture
//  Upload texture data

// ADD_CHARACTER:
//  Get metrics for the char
//  Pack the glyph into the atlas
//  Rasterize the glyph into the atlas buffer
//  Create a new texture
//  Upload the buffer into the texture