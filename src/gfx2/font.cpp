#include "font.h"
#include <base/util.h>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace gfx {
namespace internal {

// Simple RAII wrapper
struct FreetypeLibrary : public RefCountedBase {
  FreetypeLibrary() { FT_Init_FreeType(&lib); }
  ~FreetypeLibrary() { FT_Done_FreeType(lib); }
  FT_Library lib = {};
};

struct FreetypeFace {
  FreetypeFace(FT_Face face) : face(face) {}
  ~FreetypeFace() { FT_Done_Face(face); }
  FT_Face face = {};
};

}  // namespace internal

// Will leak
static internal::FreetypeLibrary* g_sharedFtLib = nullptr;

Typeface::Typeface(std::unique_ptr<internal::FreetypeFace> face)
    : ftFace_(std::move(face)) {}

Typeface::~Typeface() = default;

void Typeface::SetPixelSize(uint32_t size) {
  FT_Set_Pixel_Sizes(ftFace_->face, 0, size);
}

Expected<Glyph> Typeface::GetCharMetrics(char c) {
  auto face = ftFace_->face;
  if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
    return std::unexpected(GenericErrorCode::InvalidArg);
  }
  return Glyph{
    .character = c,
    .width = static_cast<uint32_t>(face->glyph->bitmap.width),
    .height = static_cast<uint32_t>(face->glyph->bitmap.rows),
    .bearingX = static_cast<uint32_t>(face->glyph->bitmap_left),
    .bearingY = static_cast<uint32_t>(face->glyph->bitmap_top),
    // Convert from 26.6 fixed-point
    .advance = static_cast<uint32_t>(face->glyph->advance.x >> 6),
  };
}

bool Typeface::RasterizeChar(std::span<uint8_t> buffer,
                             uint32_t rowWidth,
                             char ch,
                             int32_t offsetX,
                             int32_t offsetY) {
  FT_Face face = ftFace_->face;
  DASSERT(face && face->size);
  if (!face || !face->size) {
    return false;
  }
  // Assume grayscale 8-bit format (1 channel)
  if (FT_Load_Char(face, ch, FT_LOAD_RENDER)) {
    LOG_ERROR("RasterizeChar: Failed to load character '{}'", ch);
    return false;
  }

  FT_GlyphSlot g = face->glyph;
  if (g->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
    LOG_ERROR("RasterizeChar: Can rasterize only a grayscale font");
    return false;
  }
  // Safety check
  DASSERT_F(
    [&]() {
      const auto bufferHeight = buffer.size() / rowWidth;
      const auto isOOB = offsetX + g->bitmap.width > rowWidth ||
                         offsetY + g->bitmap.rows > bufferHeight;
      return !isOOB;
    }(),
    "Glyph placement out of buffer bounds");

  // Copy glyph bitmap into atlas at (x, y)
  for (uint32_t row = 0; row < g->bitmap.rows; ++row) {
    std::memcpy(&buffer[(offsetY + row) * rowWidth + offsetX],
                &g->bitmap.buffer[row * g->bitmap.pitch], g->bitmap.width);
  }
  return true;
}

Expected<Ref<Typeface>> Typeface::CreateFromFile(
  const std::filesystem::path& filename) {
  // Init shared Ft_library
  if (!g_sharedFtLib) {
    g_sharedFtLib = new internal::FreetypeLibrary();
  }

  AutoFreePtr<FT_FaceRec_, Deleter<FT_Done_Face>> ftFace;
  if (FT_New_Face(g_sharedFtLib->lib, filename.string().c_str(), 0, &ftFace)) {
    LOG_ERROR("OpenFont: cannot open font face '{}'", filename);
    return std::unexpected(GenericErrorCode::InternalError);
  }
  return MakeRefCounted<Typeface>(
    std::make_unique<internal::FreetypeFace>(ftFace.release()));
}

}  // namespace gfx
