#pragma once
#include <algorithm>
#include "base/common.h"

// Clamp to 0.f - 1.f range
constexpr float NormalizeFloat(float f) {
    return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f;
}

constexpr uint8_t ToInt8(float f) {
    return ((uint8_t)(NormalizeFloat(f) * 255.0f + 0.5f));
}

constexpr float ToNormalizedFloat(uint8_t i) {
    return float(i) / 255.f;
}

struct RGBAMaskOffsets {
    static constexpr uint32_t R = 24;
    static constexpr uint32_t G = 16;
    static constexpr uint32_t B = 8;
    static constexpr uint32_t A = 0;
};

constexpr float GetFloatFromIntRGBA(uint32_t val, uint32_t offset) {
    const float fl = (float)((val >> offset) & 0xffu) / 255.f;
    return NormalizeFloat(fl);
}

// RGBA color
// 0 - 1 normalized
class Color4f {
public:
    float r, g, b, a;

    constexpr Color4f() : r(0), g(0), b(0), a(1.f) {}

    constexpr Color4f(uint32_t val)
        : r(GetFloatFromIntRGBA(val, RGBAMaskOffsets::R))
        , g(GetFloatFromIntRGBA(val, RGBAMaskOffsets::G))
        , b(GetFloatFromIntRGBA(val, RGBAMaskOffsets::B))
        , a(GetFloatFromIntRGBA(val, RGBAMaskOffsets::A)) {}

    constexpr Color4f(float val)
        : r(std::clamp(val, 0.f, 1.f))
        , g(std::clamp(val, 0.f, 1.f))
        , b(std::clamp(val, 0.f, 1.f))
        , a(1.0f) {}

    constexpr Color4f(float r, float g, float b, float a = 1.f)
        : r(std::clamp(r, 0.f, 1.f))
        , g(std::clamp(g, 0.f, 1.f))
        , b(std::clamp(b, 0.f, 1.f))
        , a(std::clamp(a, 0.f, 1.f)) {}

    constexpr static Color4f FromHex(std::string_view str) {
        DASSERT(str.starts_with('#'));
        DASSERT(str.size() == 7 || str.size() == 9);
        const int numComponents = ((int)str.size() - 1) / 2;
        const char* ptr = str.data() + 1;
        uint8_t out[4]{};
        // Set alpha to 1
        out[3] = 0xFF;
        for (int i = 0; i < numComponents; ++i) {
            const std::from_chars_result result =
                std::from_chars(ptr, ptr + 2, out[i], 16);
            DASSERT(result.ec == std::errc());
            ptr += 2;
        }
        return {out[0] / 255.f, out[1] / 255.f, out[2] / 255.f, out[3] / 255.f};
    }

    constexpr float& operator[](size_t i) {
        switch (i) {
            case 0: return r;
            case 1: return g;
            case 2: return b;
            case 3: return a;
            default: return operator[](i % 4);
        }
    }

    constexpr uint32_t ToABGR8Unorm() const {
        return (((uint32_t)ToInt8(r)) << RGBAMaskOffsets::A) |
               (((uint32_t)ToInt8(g)) << RGBAMaskOffsets::B) |
               (((uint32_t)ToInt8(b)) << RGBAMaskOffsets::G) |
               (((uint32_t)ToInt8(a)) << RGBAMaskOffsets::R);
    }

    constexpr uint32_t ToRGBA8Unorm() const {
        return (((uint32_t)ToInt8(r)) << RGBAMaskOffsets::R) |
               (((uint32_t)ToInt8(g)) << RGBAMaskOffsets::G) |
               (((uint32_t)ToInt8(b)) << RGBAMaskOffsets::B) |
               (((uint32_t)ToInt8(a)) << RGBAMaskOffsets::A);
    }

    constexpr float R() const { return r; }
    constexpr float G() const { return g; }
    constexpr float B() const { return b; }
    constexpr float A() const { return a; }
};

/**
 * RGBA color stored as a 32bit integer
 * Component ranges 0 to ff
 */
struct ColorU32 {
public:
    constexpr ColorU32() : rgb(0xff) {}

    constexpr ColorU32(float R, float G, float B, float A) : rgb() {
        rgb |= ((uint32_t)ToInt8(R)) << RGBAMaskOffsets::R;
        rgb |= ((uint32_t)ToInt8(G)) << RGBAMaskOffsets::G;
        rgb |= ((uint32_t)ToInt8(B)) << RGBAMaskOffsets::B;
        rgb |= ((uint32_t)ToInt8(A)) << RGBAMaskOffsets::A;
    }

    constexpr ColorU32(const Color4f& col)
        : ColorU32(col.r, col.g, col.b, col.a) {}

    constexpr static ColorU32 FromHex(std::string_view inStr) {
        return {Color4f::FromHex(inStr)};
    }

    constexpr ColorU32& operator=(std::string_view inHexCode) {
        *this = Color4f::FromHex(inHexCode);
        return *this;
    }

    constexpr float R() const {
        return ToNormalizedFloat(Extract(RGBAMaskOffsets::R));
    }

    constexpr float G() const {
        return ToNormalizedFloat(Extract(RGBAMaskOffsets::G));
    }

    constexpr float B() const {
        return ToNormalizedFloat(Extract(RGBAMaskOffsets::B));
    }

    constexpr float A() const {
        return ToNormalizedFloat(Extract(RGBAMaskOffsets::A));
    }

    constexpr void SetR(float val) { SetComponent(RGBAMaskOffsets::R, val); }
    constexpr void SetG(float val) { SetComponent(RGBAMaskOffsets::G, val); }
    constexpr void SetB(float val) { SetComponent(RGBAMaskOffsets::B, val); }
    constexpr void SetA(float val) { SetComponent(RGBAMaskOffsets::A, val); }

    constexpr void SetComponent(uint32_t offset, float val) {
        const uint8_t i = ToInt8(val);
        rgb &= ~(0xFF << offset);
        rgb |= i << offset;
    }

    constexpr operator uint32_t() const { return rgb; }

private:
    constexpr uint8_t Extract(uint32_t offset) const {
        return (uint8_t)((rgb >> offset) & 0xFF);
    }

private:
    uint32_t rgb;
};