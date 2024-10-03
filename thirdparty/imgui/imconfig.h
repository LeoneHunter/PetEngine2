// Should be copied into /imgui when building
#pragma once
#include "src/base/math_util.h"

#define IM_VEC2_CLASS_EXTRA                                                     \
        constexpr ImVec2(const float2& f) : x(f.x), y(f.y) {}                   \
        operator float2() const { return float2(x,y); }							\

#define IM_VEC4_CLASS_EXTRA                                                     \
        constexpr ImVec4(const float4& f) : x(f.x), y(f.y), z(f.z), w(f.w) {}   \
        operator float4() const { return float4(x,y,z,w); }

#define ImDrawIdx uint32_t
