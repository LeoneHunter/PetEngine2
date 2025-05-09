#ifndef FONT_H
#define FONT_H

#include "common.h"

namespace gfx {

namespace internal {
struct FreetypeLibrary;
struct FreetypeFace;
}  // namespace internal

// Glyph data for ASCII
struct Glyph {
  char character;
  uint32_t width, height;
  uint32_t bearingX, bearingY;
  uint32_t advance;
};

// Wraps actual font data
class Typeface : public RefCountedBase {
public:
  // Synchronously loads a font file from disk
  static Expected<Ref<Typeface>> CreateFromFile(
    const std::filesystem::path& filename);

  // Set cached pixel size for subsequent queries
  void SetPixelSize(uint32_t size);

  // Read character metrics
  Expected<Glyph> GetCharMetrics(char c);

  // Rasterize the glyph into the buffer at position offsetX:offsetY
  // Assumes a 2D buffer
  bool RasterizeChar(std::span<uint8_t> buffer,
                     uint32_t rowWidth,
                     char c,
                     int32_t offsetX,
                     int32_t offsetY);

public:
  Typeface(std::unique_ptr<internal::FreetypeFace> face);
  ~Typeface();

  Typeface(Typeface&&) = default;
  Typeface& operator=(Typeface&&) = default;

private:
  std::unique_ptr<internal::FreetypeFace> ftFace_;
};

}  // namespace gfx

#endif FONT_H