#pragma once
#include "runtime/common.h"
#include "runtime/util.h"

#include <span>

namespace gfx {

// 2D point
class Point {
public:
	constexpr			Point(): x(0), y(0) {}
	constexpr explicit	Point(float in): x(in), y(in) {}
	constexpr			Point(float inX, float inY) : x(inX), y(inY) {}

	constexpr float& operator[](uint8_t inAxisIndex) noexcept {
		switch(inAxisIndex) {
			case 0: return x;
			case 1: return y;
		}
		DASSERT(false && "Index out of bounds");
		return x;
	}

	constexpr float const& operator[](uint8_t inAxisIndex) const {
		switch(inAxisIndex) {
			case 0: return x;
			case 1: return y;
		}
		DASSERT(false && "Index out of bounds");
		return x;
	}

	constexpr float& operator[](Axis inAxis) noexcept {
		switch(inAxis) {
			case Axis::X: return x;
			case Axis::Y: return y;
		}
		DASSERT(false && "Index out of bounds");
		return x;
	}

	constexpr float const& operator[](Axis inAxis) const {
		switch(inAxis) {
			case Axis::X: return x;
			case Axis::Y: return y;
		}
		DASSERT(false && "Index out of bounds");
		return x;
	}

	constexpr static Point Max() { return {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}; }
	constexpr static Point Min() { return {std::numeric_limits<float>::min(), std::numeric_limits<float>::min()}; }
	constexpr static Point Zero() { return {0.f, 0.f}; }

    constexpr friend Point operator+(const Point& lhs, const Vec2f& rhs) {
        return {lhs.x + rhs.x, lhs.y + rhs.y};
    }

    constexpr friend Point operator-(const Point& lhs, const Vec2f& rhs) {
        return {lhs.x - rhs.x, lhs.y - rhs.y};
    }

public:
	float x;
	float y;
};

using ::Vec2f;


class Size {
public: 



};

// RGBA float color
class Color4f {
public:


    constexpr Color4f() = default;

    constexpr static Color4f FromRGBA(float r, float g, float b, float a) {
        return {r, g, b, a};
    }

    constexpr static Color4f FromHexCode(uint32_t code) {
        return {
            ((code & 0xFF000000) >> 24) / 255.f,
            ((code & 0x00FF0000) >> 16) / 255.f,
            ((code & 0x0000FF00) >> 8) / 255.f,
            ((code & 0x000000FF) >> 0) / 255.f
        };
    }

    constexpr static Color4f FromHexCode(std::string_view str) {
        DASSERT(str.starts_with('#'));
        DASSERT(str.size() == 7 || str.size() == 9);
        const int numComponents = ((int)str.size() - 1) / 2;
        const char* ptr = str.data() + 1;
        uint8_t out[4]{};
        for (int i = 0; i < numComponents; ++i) {
            const std::from_chars_result result =
                std::from_chars(ptr, ptr + 2, out[i], 16);
            DASSERT(result.ec == std::errc());
            ptr += 2;
        }
        return {out[0] / 255.f, out[1] / 255.f, out[2] / 255.f, out[3] / 255.f};
    }

    constexpr float GetAlpha() const { return a; }

    constexpr uint32_t ToRGBAU32() const {
        uint32_t out = 0;
        out |= uint8_t(r * 255.f) << 24;
        out |= uint8_t(g * 255.f) << 16;
        out |= uint8_t(b * 255.f) << 8;
        out |= uint8_t(a * 255.f) << 0;
        return out;
    }

private:    
    
    constexpr Color4f(float r, float g, float b, float a)
        : r(r), g(g), b(b), a(a) 
    {}

    float r, g, b, a;
};

using Color32 = uint32_t;

// Min is Top Left
class Rect {
public:

    static Rect FromTLWH(Point min, Size size);
    static Rect FromTLWH(float x, float y, float w, float h);

    static Rect FromTLBR(Point min, Point max);
    static Rect FromTLBR(float minX, float minY, float maxX, float maxY);

    static Rect FromTRBL(float top, float right, float bottom, float left);
    static Rect FromPoints(const Point& p1, const Point& p2);

    Point TL() const { return min; }
    Point TR() const { return {max.x, min.y}; }
    Point BR() const { return max; }
    Point BL() const { return {min.x, max.y}; }

    Point Min() const { return min; }
    Point Max() const { return max; }

    Rect Translate(const Vec2f& vec) const {
        return Rect::FromPoints(min + vec, max + vec);
    }

    // Ensure than min < max
    void Normalize() {
        if(min.x > max.x) {
            std::swap(min.x, max.x);
        }
        if(min.y > max.y) {
            std::swap(min.y, max.y);
        }
    }

private:
    Point min;
    Point max;
};

// 4x4 row-major matrix
class Mat44f {
public:
    constexpr Mat44f() : data{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1} {}

    constexpr Mat44f(const Mat44f&) = default;
    constexpr Mat44f& operator=(const Mat44f&) = default;

    constexpr static Mat44f CreateOrthographic(float top,
                                               float right,
                                               float bottom,
                                               float left) {
        Mat44f out;
        out.data = {
            2.0f / (right - left), 0.0f, 0.0f, 0.0f,
            0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            (right + left) / (left - right), (top + bottom) / (bottom - top), 0.5f, 1.0f
        };
        return out;
    }

private:
    std::array<float, 16> data;
};

namespace colors {

constexpr Color4f kRed = Color4f::FromHexCode("#ff0000");
constexpr Color4f kWhite = Color4f::FromHexCode("#000000");

} // namespace colors

enum class Corners {
    All = 0,
    TopLeft = 0x01,
    TopRight = 0x02,
    BottomRight = 0x04,
    BottomLeft = 0x08,
};
DEFINE_ENUM_FLAGS_OPERATORS(Corners);


} // namespace gfx