#pragma once
#include <array>
#include <format>
#include <numeric>
#include <random>

#include "error.h"

enum class Axis : uint8_t { X, Y };

constexpr Axis FlipAxis(Axis axis) {
    return axis == Axis::X ? Axis::Y : Axis::X;
}
constexpr Axis operator!(Axis axis) {
    return FlipAxis(axis);
}
// For easy iteration
constexpr std::array<Axis, 2> axes2D{Axis::X, Axis::Y};

/**
 * Stores vector of 2 values
 * Used for 2D coordinates and 2D sizes
 */
template <class T>
class Vec2 {
public:
    constexpr Vec2() : x(0), y(0) {}
    constexpr explicit Vec2(T in) : x(in), y(in) {}
    constexpr Vec2(T inX, T inY) : x(inX), y(inY) {}

    template <typename OtherT>
    constexpr Vec2(const Vec2<OtherT>& in)
        : x(static_cast<T>(in.x)), y(static_cast<T>(in.y)) {}

    template <typename OtherT>
    constexpr Vec2<T>& operator=(const Vec2<OtherT>& in) {
        x = static_cast<T>(in.x);
        y = static_cast<T>(in.y);
        return *this;
    }

    constexpr T& operator[](uint8_t inAxisIndex) noexcept {
        switch (inAxisIndex) {
            case 0:
                return x;
            case 1:
                return y;
        }
        DASSERT(false && "Index out of bounds");
        return x;
    }

    constexpr T const& operator[](uint8_t inAxisIndex) const {
        switch (inAxisIndex) {
            case 0:
                return x;
            case 1:
                return y;
        }
        DASSERT(false && "Index out of bounds");
        return x;
    }

    constexpr T& operator[](Axis inAxis) noexcept {
        switch (inAxis) {
            case Axis::X:
                return x;
            case Axis::Y:
                return y;
        }
        DASSERT(false && "Index out of bounds");
        return x;
    }

    constexpr T const& operator[](Axis inAxis) const {
        switch (inAxis) {
            case Axis::X:
                return x;
            case Axis::Y:
                return y;
        }
        DASSERT(false && "Index out of bounds");
        return x;
    }

    constexpr static Vec2<T> Max() {
        return {std::numeric_limits<T>::max(), std::numeric_limits<T>::max()};
    }
    constexpr static Vec2<T> Min() {
        return {std::numeric_limits<T>::min(), std::numeric_limits<T>::min()};
    }
    constexpr static Vec2<T> Zero() { return {0.f, 0.f}; }

public:
    T x;
    T y;
};

template <class T>
struct std::formatter<Vec2<T>, char> {
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <class FmtContext>
    FmtContext::iterator format(Vec2<T> t, FmtContext& ctx) const {
        const auto text = std::format("{:.1f} {:.1f}", t.x, t.y);
        return std::ranges::copy(std::move(text), ctx.out()).out;
    }
};

using float2 = Vec2<float>;
using uint2 = Vec2<uint32_t>;
using uint64_2 = Vec2<uint64_t>;
using int2 = Vec2<int32_t>;

using Vec2f = Vec2<float>;

template <typename T>
concept Number = std::is_arithmetic_v<T>;

template <Number T>
constexpr Vec2<T> operator*(T left, const Vec2<T>& right) {
    return Vec2<T>(left * right.x, left * right.y);
}
template <Number T>
constexpr Vec2<T> operator*(const Vec2<T>& left, T right) {
    return Vec2<T>(left.x * right, left.y * right);
}
template <Number T>
constexpr Vec2<T> operator+(float left, Vec2<T> right) {
    return Vec2<T>(left + right.x, left + right.y);
}
template <Number T>
constexpr Vec2<T> operator+(const Vec2<T>& left, T right) {
    return Vec2<T>(left.x + right, left.y + right);
}
template <Number T>
constexpr Vec2<T> operator-(T left, const Vec2<T>& right) {
    return Vec2<T>(left - right.x, left - right.y);
}
template <Number T>
constexpr Vec2<T> operator-(const Vec2<T>& left, T right) {
    return Vec2<T>(left.x - right, left.y - right);
}
template <Number T>
constexpr Vec2<T> operator/(const Vec2<T>& left, T right) {
    return Vec2<T>(left.x / right, left.y / right);
}

template <Number T>
constexpr Vec2<T> operator*(const Vec2<T>& left, const Vec2<T>& right) {
    return Vec2<T>(left.x * right.x, left.y * right.y);
}
template <Number T>
constexpr Vec2<T> operator/(const Vec2<T>& left, const Vec2<T>& right) {
    return Vec2<T>(left.x / right.x, left.y / right.y);
}
template <Number T>
constexpr Vec2<T> operator-(const Vec2<T>& left, const Vec2<T>& right) {
    return Vec2<T>(left.x - right.x, left.y - right.y);
}
template <Number T>
constexpr Vec2<T> operator+(const Vec2<T>& left, const Vec2<T>& right) {
    return Vec2<T>(left.x + right.x, left.y + right.y);
}
template <Number T>
constexpr Vec2<T>& operator+=(Vec2<T>& left, const Vec2<T>& right) {
    left.x += right.x;
    left.y += right.y;
    return left;
}
template <Number T>
constexpr Vec2<T>& operator-=(Vec2<T>& left, const Vec2<T>& right) {
    left.x -= right.x;
    left.y -= right.y;
    return left;
}
template <Number T>
constexpr Vec2<T>& operator-=(Vec2<T>& left, T right) {
    left.x -= right;
    left.y -= right;
    return left;
}
template <Number T>
constexpr Vec2<T>& operator+=(Vec2<T>& left, T right) {
    left.x += right;
    left.y += right;
    return left;
}
template <Number T>
constexpr Vec2<T> operator-(const Vec2<T>& right) {
    return Vec2<T>(-right.x, -right.y);
}
template <Number T>
constexpr bool operator==(const Vec2<T>& left, const Vec2<T>& right) {
    return left.x == right.x && left.y == right.y;
}

template <Number T1, Number T2>
constexpr Vec2<T1> operator-(const Vec2<T1>& left, const Vec2<T2>& right) {
    return Vec2<T1>(left.x - right.x, left.y - right.y);
}
template <Number T1, Number T2>
constexpr Vec2<T1> operator+(const Vec2<T1>& left, const Vec2<T2>& right) {
    return Vec2<T1>(left.x + right.x, left.y + right.y);
}



// 4 axis vector
// Could represent a point or a size
class Vec4 {
public:
    constexpr Vec4() : x(0), y(0), z(0), w(0) {}
    constexpr explicit Vec4(float in) : x(in), y(in), z(in), w(in) {}
    constexpr Vec4(float inX, float inY, float inZ, float inW)
        : x(inX), y(inY), z(inZ), w(inW) {}

    constexpr float& operator[](size_t i) noexcept {
        switch (i) {
            default:
                return x;
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            case 3:
                return w;
        }
    }

    constexpr float const& operator[](size_t i) const {
        switch (i) {
            default:
                return x;
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            case 3:
                return w;
        }
    }

public:
    float x;
    float y;
    float z;
    float w;
};

constexpr bool operator==(const Vec4& left, const Vec4& right) {
    return left.x == right.x && left.y == right.y && left.z == right.z &&
           left.w == right.w;
}

constexpr Vec4 operator+(const Vec4& left, const Vec4& right) {
    Vec4 out;
    out.x = left.x + right.x;
    out.y = left.y + right.y;
    out.z = left.z + right.z;
    out.w = left.w + right.w;
    return out;
}

constexpr Vec4 operator*(const Vec4& left, float right) {
    Vec4 out;
    out.x = left.x * right;
    out.y = left.y * right;
    out.z = left.z * right;
    out.w = left.w * right;
    return out;
}

constexpr Vec4& operator*=(Vec4& left, float right) {
    left.x *= right;
    left.y *= right;
    left.z *= right;
    left.w *= right;
    return left;
}

constexpr Vec4 operator-(const Vec4& left) {
    return {-left.x, -left.y, -left.z, -left.w};
}

using float4 = Vec4;



// Side of the rect.
// Used as an index to get rect side
enum Side { SideTop, SideRight, SideBottom, SideLeft };


/*
 * Alternative Rect representation
 * Can store various rect parameters
 * margins, paddings, border sizes
 */
template <typename T>
struct RectSides {
    using value_type = T;

    value_type Top;
    value_type Right;
    value_type Bottom;
    value_type Left;

    constexpr RectSides(value_type val = 0) {
        Left = val;
        Right = val;
        Top = val;
        Bottom = val;
    }

    constexpr RectSides(value_type horizontal, value_type vertical) {
        Left = horizontal;
        Right = horizontal;
        Top = vertical;
        Bottom = vertical;
    }

    // TRouBLe
    constexpr RectSides(value_type top,
                        value_type right,
                        value_type bottom,
                        value_type left) {
        Left = left;
        Right = right;
        Top = top;
        Bottom = bottom;
    }

    constexpr value_type Vertical() const { return Top + Bottom; }

    constexpr value_type Horizontal() const { return Left + Right; }

    constexpr Vec2<value_type> TL() const { return {Left, Top}; }

    constexpr Vec2<value_type> BR() const { return {Right, Bottom}; }

    // Combined size of margins Left + Right Top + Bottom
    constexpr Vec2<value_type> Size() const {
        return {Left + Right, Top + Bottom};
    }

    constexpr bool Empty() const { return !Left && !Right && !Top && !Bottom; }

    constexpr value_type& operator[](Side inSide) {
        switch (inSide) {
            case SideTop:
                return Top;
            case SideBottom:
                return Bottom;
            case SideLeft:
                return Left;
            case SideRight:
                return Right;
        }
        return Left;
    }

    constexpr value_type operator[](Side inSide) const {
        switch (inSide) {
            case SideTop:
                return Top;
            case SideBottom:
                return Bottom;
            case SideLeft:
                return Left;
            case SideRight:
                return Right;
        }
        return Left;
    }
};



/**
 * Axis aligned rectangle
 * same as AABB
 */
struct Rect {
    using value_type = float;
    using point_type = Vec2<value_type>;

    point_type min;  // Upper-left
    point_type max;  // Lower-right

    constexpr Rect() : min(0.0f, 0.0f), max(0.0f, 0.0f) {}
    constexpr explicit Rect(point_type size) : min(0.f), max(size) {}
    constexpr Rect(point_type min, point_type size)
        : min(min), max(min + size) {}
    constexpr Rect(point_type center, value_type width)
        : min(center - width), max(center + width) {}
    constexpr Rect(value_type x1, value_type y1, value_type x2, value_type y2)
        : min(x1, y1), max(x2, y2) {}

    constexpr point_type Origin() const { return min; }
    constexpr point_type Center() const {
        return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
    }
    constexpr point_type Size() const { return {max.x - min.x, max.y - min.y}; }
    constexpr value_type Width() const { return max.x - min.x; }
    constexpr value_type Height() const { return max.y - min.y; }

    constexpr value_type& Side(int inSide) {
        switch (inSide) {
            case SideTop:
                return min.y;
            case SideBottom:
                return max.y;
            case SideLeft:
                return min.x;
            case SideRight:
                return max.x;
        }
        return min.y;
    }

    constexpr const value_type& Side(int inSide) const {
        switch (inSide) {
            case SideTop:
                return min.y;
            case SideBottom:
                return max.y;
            case SideLeft:
                return min.x;
            case SideRight:
                return max.x;
        }
        return min.y;
    }

    // Convenience methods
    constexpr value_type& Top() { return min.y; }
    constexpr value_type& Bottom() { return max.y; }
    constexpr value_type& Left() { return min.x; }
    constexpr value_type& Right() { return max.x; }

    constexpr value_type Top() const { return min.y; }
    constexpr value_type Bottom() const { return max.y; }
    constexpr value_type Left() const { return min.x; }
    constexpr value_type Right() const { return max.x; }

    constexpr point_type TL() const { return min; }
    constexpr point_type BR() const { return max; }
    constexpr point_type TR() const { return min + point_type{Width(), 0.f}; }
    constexpr point_type BL() const { return max - point_type{Width(), 0.f}; }

    // Build rect from two arbitrarily positioned points
    constexpr void BuildFromPoints(point_type inP0, point_type inP1);

    constexpr bool Contains(point_type inPoint) const {
        return inPoint.x > min.x && inPoint.x < max.x && inPoint.y > min.y &&
               inPoint.y < max.y;
    }

    constexpr bool Intersects(const Rect& otherRect) const {
        if (max[0] < otherRect.min[0] || min[0] > otherRect.max[0])
            return false;
        if (max[1] < otherRect.min[1] || min[1] > otherRect.max[1])
            return false;
        return true;
    }

    constexpr void ClosestPoint(point_type inPoint,
                                point_type& outClosest) const {
        for (uint8_t i = 0; i < 2; i++) {
            auto v = inPoint[i];
            if (v < min[i])
                v = min[i];
            if (v > max[i])
                v = max[i];
            outClosest[i] = v;
        }
    }

    constexpr Rect& SetOrigin(point_type inNewOrigin) {
        *this = Rect(inNewOrigin, inNewOrigin + Size());
        return *this;
    }

    constexpr Rect& SetSize(point_type inSize) {
        max = min + inSize;
        return *this;
    }

    constexpr Vec4 ToFloat4() const { return {min.x, min.y, max.x, max.y}; }

    constexpr void Clear() {
        min = point_type{0.f};
        max = point_type{0.f};
    }
    // If min == max the rect is technically empty
    constexpr bool Empty() const { return min == max; }

    // Helpers
    constexpr Rect& Translate(point_type inTranslation) {
        min += inTranslation;
        max += inTranslation;
        return *this;
    }

    constexpr Rect& Translate(value_type inTranslationX,
                              value_type inTranslationY) {
        min += point_type(inTranslationX, inTranslationY);
        max += point_type(inTranslationX, inTranslationY);
        return *this;
    }

    // Left to left
    constexpr Rect& AlignLeft(const Rect& target, value_type margin = 0.f) {
        const auto width = this->Width();
        min.x = target.min.x + margin;
        max.x = min.x + width;
        return *this;
    }

    // Right to right
    constexpr Rect& AlignRight(const Rect& target, value_type margin = 0.f) {
        const auto width = this->Width();
        max.x = target.max.x - margin;
        min.x = max.x - width;
        return *this;
    }

    // Horizontally center to center
    constexpr Rect& AlignHorizontalCenter(const Rect& target) {
        const auto width = this->Width();
        min.x = target.min.x + (target.Width() - width) * 0.5f;
        max.x = min.x + width;
        return *this;
    }

    // Top to top
    constexpr Rect& AlignTop(const Rect& target, value_type margin = 0.f) {
        const auto height = this->Height();
        min.y = target.min.y + margin;
        max.y = min.y + height;
        return *this;
    }

    constexpr Rect& AlignBottom(const Rect& target, value_type margin = 0.f) {
        const auto height = this->Height();
        max.y = target.max.y - margin;
        min.y = max.y - height;
        return *this;
    }

    constexpr Rect& AlignVerticalCenter(const Rect& target) {
        const auto height = this->Height();
        min.y = target.min.y + (target.Height() - height) * 0.5f;
        max.y = min.y + height;
        return *this;
    }

    constexpr Rect& AlignCenter(const Rect& target) {
        AlignHorizontalCenter(target);
        AlignVerticalCenter(target);
        return *this;
    }

    constexpr Rect& Expand(value_type val) {
        min -= val;
        max += val;
        return *this;
    }

    constexpr Rect& Expand(value_type horizontal, value_type vertical) {
        min.x -= horizontal;
        max.x += horizontal;
        min.y -= vertical;
        max.y += vertical;
        return *this;
    }

    constexpr Rect& Expand(value_type top,
                           value_type right,
                           value_type bottom,
                           value_type left) {
        min.x -= left;
        max.x += right;
        min.y -= top;
        max.y += bottom;
        return *this;
    }

    constexpr Rect& Expand(Vec4 vec) {
        min.y -= vec.x;
        max.x += vec.y;
        max.y += vec.z;
        min.x -= vec.w;
        return *this;
    }

    template <typename T>
    constexpr Rect& Expand(const RectSides<T> sides) {
        min.y -= sides.Top;
        max.x += sides.Right;
        max.y += sides.Bottom;
        min.x -= sides.Left;
        return *this;
    }
};

// Axis aligned bounding box
using AABB = Rect;

constexpr Rect operator+(const Rect& lhs, const float2& rhs) {
    return {lhs.min + rhs, lhs.max + rhs};
}
constexpr Rect operator-(const Rect& lhs, const float2& rhs) {
    return {lhs.min - rhs, lhs.max - rhs};
}

constexpr Rect& operator+=(Rect& lhs, const float2& rhs) {
    lhs.min += rhs;
    lhs.max += rhs;
    return lhs;
}
constexpr Rect& operator-=(Rect& lhs, const float2& rhs) {
    lhs.min -= rhs;
    lhs.max -= rhs;
    return lhs;
}

constexpr bool operator==(const Rect& lhs, const Rect& rhs) {
    return lhs.min == rhs.min && lhs.max == rhs.max;
}
constexpr bool operator!=(const Rect& lhs, const Rect& rhs) {
    return lhs.min != rhs.min || lhs.max != rhs.max;
}

constexpr void swap(float2& left, float2& right) {
    const auto temp = left;
    left = right;
    right = temp;
}

constexpr Rect RectAlignHorizontalLeft(const Rect& target,
                                       const Rect& object,
                                       float margin = 0.f) {
    auto temp = object;
    temp.min.x = target.min.x + margin;
    temp.max.x = temp.min.x + object.Width();
    return temp;
}

constexpr Rect RectAlignHorizontalRight(const Rect& target,
                                        const Rect& object,
                                        float margin = 0.f) {
    auto temp = object;
    temp.max.x = target.max.x - margin;
    temp.min.x = temp.max.x - object.Width();
    return temp;
}

constexpr Rect RectAlignHorizontalCenter(const Rect& target,
                                         const Rect& object) {
    auto temp = object;
    temp.min.x = target.min.x + (target.Width() - object.Width()) * 0.5f;
    temp.max.x = temp.min.x + object.Width();
    return temp;
}

constexpr Rect RectAlignVerticalTop(const Rect& target,
                                    const Rect& object,
                                    float margin = 0.f) {
    auto temp = object;
    temp.min.y = target.min.y + margin;
    temp.max.y = temp.min.y + object.Height();
    return temp;
}

constexpr Rect RectAlignVerticalBottom(const Rect& target,
                                       const Rect& object,
                                       float margin = 0.f) {
    auto temp = object;
    temp.max.y = target.max.y - margin;
    temp.min.y = temp.max.y - object.Height();
    return temp;
}

constexpr Rect RectAlignVerticalCenter(const Rect& target, const Rect& object) {
    auto temp = object;
    temp.min.y = target.min.y + (target.Height() - object.Height()) * 0.5f;
    temp.max.y = temp.min.y + object.Height();
    return temp;
}



template <typename T>
struct Circle {
    using value_type = T;
    using point_type = Vec2<value_type>;

    point_type center;
    value_type radius;

    constexpr Circle() : center(0.0f, 0.0f), radius(0.0f) {}
    constexpr Circle(const point_type& center, value_type radius)
        : center(center), radius(radius) {}
    constexpr Circle(value_type p0, value_type p1, value_type radius)
        : center(p0, p1), radius(radius) {}

    constexpr value_type size() const { return radius * 2; }
};



namespace math {

constexpr float dot(const float2& inVecLeft, const float2& inVecRight) {
    float2 tmp(inVecLeft * inVecRight);
    return tmp.x + tmp.y;
}

template <Number T>
constexpr T Abs(T inVal) {
    return inVal < T() ? -inVal : inVal;
}

template <Number T>
constexpr Vec2<T> Abs(Vec2<T> inVec) {
    return Vec2<T>(std::abs(inVec.x), std::abs(inVec.y));
}

template <Number T>
constexpr T Clamp(T inVal,
                  T inMin = std::numeric_limits<T>::min(),
                  T inMax = std::numeric_limits<T>::max()) {
    return (inVal < inMin) ? inMin : (inVal > inMax) ? inMax : inVal;
}

constexpr float2 Clamp(float2 inVal,
                       float2 inMin = float2(FLT_MIN),
                       float2 inMax = float2(FLT_MAX)) {
    float2 out;
    for (uint8_t axis = 0; axis != 2; ++axis) {
        out[axis] = inVal[axis] < inMin[axis]   ? inMin[axis]
                    : inVal[axis] > inMax[axis] ? inMax[axis]
                                                : inVal[axis];
    }
    return out;
}

constexpr float2 Clamp(float2 inVal,
                       float inMin = FLT_MIN,
                       float inMax = FLT_MAX) {
    float2 out;
    for (uint8_t axis = 0; axis != 2; ++axis) {
        out[axis] = inVal[axis] < inMin   ? inMin
                    : inVal[axis] > inMax ? inMax
                                          : inVal[axis];
    }
    return out;
}

constexpr Vec4 Clamp(Vec4 inVal, float inMin = FLT_MIN, float inMax = FLT_MAX) {
    Vec4 out;
    for (uint8_t axis = 0; axis != 4; ++axis) {
        out[axis] = inVal[axis] < inMin   ? inMin
                    : inVal[axis] > inMax ? inMax
                                          : inVal[axis];
    }
    return out;
}

// Returns point on the segment inSegment projected from inPoint
constexpr void closestPointSegment(float2 inSegmentP0,
                                   float2 inSegmentP1,
                                   float2 inPoint,
                                   float2& outClosest) {
    auto ab = inSegmentP1 - inSegmentP0;
    auto t = dot(inPoint - inSegmentP0, ab) / dot(ab, ab);
    // If outside segment, clamp t (and therefore d) to the closest endpoint
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    // Compute projected position from the clamped t
    outClosest = inSegmentP0 + (t * ab);
}

template <typename VectorType>
inline VectorType Round(VectorType in) {
    return {floor(in.x), floor(in.y)};
}

template <typename T>
constexpr T Max(T lhs, T rhs) {
    return lhs > rhs ? lhs : rhs;
}

template <typename T>
constexpr T Min(T lhs, T rhs) {
    return lhs < rhs ? lhs : rhs;
}

template <typename T>
constexpr bool Intersects(const Rect& inRect, const Vec2<T>& inPoint) {
    return inPoint.x > inRect.min.x && inPoint.x < inRect.max.x &&
           inPoint.y > inRect.min.y && inPoint.y < inRect.max.y;
}

constexpr bool Intersects(const Rect& inRectLeft, const Rect& inRectRight) {
    return Intersects(inRectLeft, inRectRight.min) ||
           Intersects(inRectLeft, inRectRight.max) ||
           Intersects(inRectLeft, inRectRight.BL()) ||
           Intersects(inRectLeft, inRectRight.TR());
}

template <typename T>
constexpr bool Intersects(const Rect& inRect, const Circle<T>& inCircle) {
    float2 cornerClosest;
    inRect.ClosestPoint(inCircle.center, cornerClosest);
    auto distance = inCircle.center - cornerClosest;
    return math::dot(distance, distance) <= inCircle.radius * inCircle.radius;
}

template <typename T>
constexpr bool TestSegmentAABB(Vec2<T> p0, Vec2<T> p1, Rect b) {
    // From Real-Time Collision Detection by Christer Ericson, published by
    // Morgan Kaufmann Publishers, � 2005 Elsevier Inc.
    constexpr float EPSILON = 1e-30f;

    auto e = b.max - b.min;
    auto d = p1 - p0;
    auto m = p0 + p1 - b.min - b.max;

    // Try world coordinate axes as separating axes
    float adx = math::Abs(d.x);
    if (math::Abs(m.x) > e.x + adx)
        return false;
    float ady = std::abs(d.y);
    if (math::Abs(m.y) > e.y + ady)
        return false;
    // Add in an epsilon term to counteract arithmetic errors when segment is
    // (near) parallel to a coordinate axis (see text for detail)
    adx += EPSILON;
    ady += EPSILON;
    // Try cross products of segment direction vector with coordinate axes
    if (math::Abs(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx)
        return false;
    // No separating axis found; segment must be overlapping AABB
    return true;
}


template <class T>
    requires std::integral<T>
T RandomInteger(T inMin = 0, T inMax = std::numeric_limits<T>::max()) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<T> distrib(inMin, inMax);
    return distrib(gen);
}
}  // namespace math

constexpr void Rect::BuildFromPoints(point_type inP0, point_type inP1) {
    // Draw block select rect
    const bool bInvertX = inP1.x < inP0.x;
    const bool bInvertY = inP1.y < inP0.y;

    const auto rectWidth = math::Abs(inP1.x - inP0.x);
    const auto rectHeight = math::Abs(inP1.y - inP0.y);

    min = inP0;
    max = inP1;

    if (bInvertX) {
        min = min - point_type(rectWidth, 0.f);
        max = max + point_type(rectWidth, 0.f);
    }

    if (bInvertY) {
        min = min - point_type(0.f, rectHeight);
        max = max + point_type(0.f, rectHeight);
    }
}

inline void alignment_should_be_power_of_two() {}

template <class T>
    requires std::is_unsigned_v<T>
constexpr T AlignUp(T val, T alignment) {
    DASSERT(std::has_single_bit(alignment));
    return (val + alignment - 1) & ~(alignment - 1);
}

template <class T>
    requires std::is_unsigned_v<T>
constexpr T AlignDown(T val, T alignment) {
    DASSERT(std::has_single_bit(alignment));
    return (val) & ~(alignment - 1);
}