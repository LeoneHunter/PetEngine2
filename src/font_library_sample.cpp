#include "gfx2/font.h"
#include "gfx2/glyph_cache.h"

// libpng
#include <png.h>
#include <iostream>

using namespace gfx;

struct {
  uint32_t start = 32;
  uint32_t end = 126;
} static constexpr kRangeLatin;

bool SaveAtlasAsPNG(const char* filename,
                    std::span<const uint8_t> atlasData,
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

// Called from glyph cache to build the atlas
void OnBuildAtlas(std::span<const uint8_t> data, size_t rowWidth) {}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    std::cout << "Please provide: font_filename font_size output_name\n";
    return 1;
  }
  auto input = std::filesystem::path(argv[0]).parent_path() / argv[1];
  auto output = std::filesystem::path(argv[0]).parent_path() / argv[3];
  // Rasterize a font atlas
  auto fontSize = atoi(argv[2]);

  auto res = Typeface::CreateFromFile(input);
  DASSERT(res);

  Ref<Typeface> font = res.value();
  std::unique_ptr<GlyphCache> glyphCache = GlyphCache::Create();
  glyphCache->SetOnBuildHandler(
    [&](std::span<const uint8_t> data, size_t rowWidth) {
      SaveAtlasAsPNG(output.string().data(), data, rowWidth,
                     data.size() / rowWidth);
    });

  // Add font and rasterize the Latin script
  glyphCache->AddFont(font);
  glyphCache->PrecacheCharacterRange(fontSize, kRangeLatin.start,
                                     kRangeLatin.end);
}
