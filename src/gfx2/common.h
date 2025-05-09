#ifndef GFX2_COMMON_H
#define GFX2_COMMON_H

#include <base/common.h>
#include <base/error.h>
#include <base/ref_counted.h>

#include <filesystem>

namespace gfx {

template <class T>
using Ref = RefCountedPtr<T>;

enum class CoordinateSpace { Screen, NormalizedUnsigned, NormalizedSigned };

// A 2D coordinates
template <CoordinateSpace Space>
class Offset {
public:
  float x, y;
};

using ScreenOffset = Offset<CoordinateSpace::Screen>;

// A 2D rectangle
struct Rect {
  float x, y, width, height;
};


}  // namespace gfx

#endif GFX2_COMMON_H