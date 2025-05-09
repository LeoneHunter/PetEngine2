#include "glyph_cache.h"
#include <ranges>

namespace gfx {
namespace {

struct PackedGlyph {
  uint32_t x, y;
  uint32_t width, height;
};

struct AtlasPackingResult {
  uint32_t atlasWidth;
  uint32_t atlasHeight;
  std::unordered_map<char, PackedGlyph> placements;
};

AtlasPackingResult PackGlyphs(std::span<const Glyph> glyphs, int32_t rowCount) {
  constexpr uint32_t padding = 1;
  auto rows = std::vector<std::vector<Glyph>>(rowCount);
  AtlasPackingResult out;

  // Greedy row fill: tallest glyphs first
  auto sorted = std::vector<Glyph>(glyphs.begin(), glyphs.end());
  std::sort(sorted.begin(), sorted.end(),
            [](const Glyph& a, const Glyph& b) { return a.height > b.height; });

  for (size_t i = 0; i < sorted.size(); ++i) {
    rows[i % rowCount].push_back(sorted[i]);
  }

  uint32_t atlasWidth = 0;
  uint32_t atlasHeight = 0;
  std::unordered_map<char, PackedGlyph> placements;

  uint32_t y = 0;
  for (const auto& row : rows) {
    uint32_t rowHeight = 0;
    uint32_t x = 0;
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

}  // namespace

void GlyphCache::PrecacheCharacterRange(uint8_t size,
                                        Codepoint start,
                                        Codepoint end) {
  // For each glyph in the range:
  // - Read glyph metrics
  // - Pack glyphs
  // - Rasterize glyphs
  // - Call the handler to upload to vram
  std::vector<Glyph> metrics;
  // Setup font sizes
  for (Ref<Typeface>& font : fontStack_) {
    font->SetPixelSize(size);
  }
  Typeface* currentFont = nullptr;
  // Get char metrics
  for (Codepoint ch = start; ch < end; ++ch) {
    auto ok = [&]() {
      for (Ref<Typeface>& font : std::views::reverse(fontStack_)) {
        if (auto res = font->GetCharMetrics(ch); res) {
          metrics.push_back(res.value());
          currentFont = font.Get();
          return true;
        }
      }
      return false;
    }();
    if (!ok) {
      LOG_ERROR("PrecacheCharacterRange: Cannot find the char '{}' in any font",
                ch);
    }
  }
  // Pack
  // TODO: Recursively decrease the rowCount until atlas fits into some
  // predefined texture size. I.e. 512x512 or 1024x1024
  auto [atlasWidth, atlasHeight, placements] = PackGlyphs(metrics, 10);
  auto buffer = std::vector<uint8_t>(atlasHeight * atlasWidth, 0);
  // TODO: Check atlas bounds

  // Rasterize the selected glyphs into the buffer
  for (const auto& [ch, glyph] : placements) {
    const auto ok =
      currentFont->RasterizeChar(buffer, atlasWidth, ch, glyph.x, glyph.y);
    DASSERT(ok);
  }
  DASSERT(onBuildHandler_);
  onBuildHandler_(buffer, atlasWidth);
}


}  // namespace gfx