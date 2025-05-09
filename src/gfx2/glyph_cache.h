#ifndef GLYPH_CACHE
#define GLYPH_CACHE

#include <base/common.h>
#include <functional>
#include <unordered_map>

#include "font.h"

namespace gfx {
// Unicode codepoint
using Codepoint = uint32_t;

enum class GlyphID : uint32_t;

// Manages a list of font files
// Rasterizes characters on demand and builds an atlas for rendering
// Multiple font files can be added
class GlyphCache {
public:
  static std::unique_ptr<GlyphCache> Create();

  using OnBuildHandler =
    std::function<void(std::span<const uint8_t> data, size_t rowWidth)>;

  // Sets the handler to build the font atlas
  // Called when a new character or font is added
  void SetOnBuildHandler(const OnBuildHandler& handler);

  // Adds a font file to the font stack
  void AddFont(Ref<Typeface> font);

  // Adds a range of characters to precache
  void PrecacheCharacterRange(uint8_t size, Codepoint start, Codepoint end);

  // Returns texture coordinates of a glyph in the atlas
  Expected<Rect> GetGlyphRect(GlyphID glyph);

  // Try to retrieve a glyph or read it from a font
  // Returns nulopt if the glyph does not exist in
  //  the current typeface
  std::optional<Glyph> FindOrInsertGlyph(char ch, uint8_t size, Typeface* face);
  std::optional<Glyph> FindGlyph(char ch, uint8_t size, Typeface* face);

public:
  GlyphCache() = default;
  ~GlyphCache() = default;

  GlyphCache(const GlyphCache&) = delete;
  GlyphCache& operator=(const GlyphCache&) = delete;

private:
  OnBuildHandler onBuildHandler_;
  std::vector<Ref<Typeface>> fontStack_;
};


//======================================================================//
inline std::unique_ptr<GlyphCache> GlyphCache::Create() {
  return std::make_unique<GlyphCache>();
}

inline void GlyphCache::SetOnBuildHandler(const OnBuildHandler& handler) {
  DASSERT(!onBuildHandler_);
  onBuildHandler_ = handler;
}

inline void GlyphCache::AddFont(Ref<Typeface> font) {
  DASSERT(font);
  fontStack_.push_back(font);
}

}  // namespace gfx

#endif  // GLYPH_CACHE
