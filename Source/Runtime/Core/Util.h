#pragma once
#include <string>

#include "BaseTypes.h"
#include "Math.h"
#include <assert.h>

constexpr Vec4 fromHexCode(std::string_view inHexCode) {
	assert(!inHexCode.empty() && inHexCode.starts_with('#') && (inHexCode.size() == 7 || inHexCode.size() == 9));
	Vec4 out;
	out.x = 0.f;
	out.y = 0.f;
	out.z = 0.f;
	out.w = 1.f;

	int temp{};
	int out1{};
	int out2{};
	const auto arraySize = inHexCode.size();

	for(size_t i = 1; i < arraySize; i++) {
		switch(inHexCode[i]) {
		case '0': temp = 0x0; break;
		case '1': temp = 0x1; break;
		case '2': temp = 0x2; break;
		case '3': temp = 0x3; break;
		case '4': temp = 0x4; break;
		case '5': temp = 0x5; break;
		case '6': temp = 0x6; break;
		case '7': temp = 0x7; break;
		case '8': temp = 0x8; break;
		case '9': temp = 0x9; break;
		case 'A':
		case 'a': temp = 0xA; break;
		case 'B':
		case 'b': temp = 0xB; break;
		case 'C':
		case 'c': temp = 0xC; break;
		case 'D':
		case 'd': temp = 0xD; break;
		case 'E':
		case 'e': temp = 0xE; break;
		case 'F':
		case 'f': temp = 0xF; break;
		default:  temp = 0x0; break;
		}
		if(i == 1 || i == 3 || i == 5 || i == 7) {
			out1 = temp;
		} else {
			out2 = temp;
		}
		if(i == 2) {
			out.x = static_cast<float>(out1 * 16 + out2) / 256.0f;
		} else if(i == 4) {
			out.y = static_cast<float>(out1 * 16 + out2) / 256.0f;
		} else if(i == 6) {
			out.z = static_cast<float>(out1 * 16 + out2) / 256.0f;
		} else if(i == 8) {
			out.w = static_cast<float>(out1 * 16 + out2) / 256.0f;
		}
	}
	return out;
}

/*
* Compile time checked hex color string representation
*/
struct HexCodeString {
public:

	template <class Ty> requires std::convertible_to<const Ty&, std::string_view>
	consteval HexCodeString(const Ty& Str_val)
		: Str(Str_val) {
		assert(Str.size() == 7 || Str.size() == 9 && Str.starts_with('#') &&
			"Hex color wrong format. String should start with a # and contain 7 or 9 characters");
	}

	_NODISCARD constexpr std::string_view get() const {
		return Str;
	}

private:
	std::string_view Str;
};

constexpr float Saturate(float f) { return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f; }
constexpr int F32toInt8(float f) { return ((int)(Saturate(f) * 255.0f + 0.5f)); }

/**
 * RGB color stored as a 32bit integer
 * Component ranges 0 to ff
 */
struct ColorU32 {
public:

	enum BitOffsets {
		R = 0,
		G = 8,
		B = 16,
		A = 24
	};

	constexpr ColorU32(): rgb(0) {}

	constexpr ColorU32(HexCodeString inHexColor)
		: rgb(0) 
	{
		auto result = fromHexCode(inHexColor.get());

		rgb = ((u32)F32toInt8(result.x)) << BitOffsets::R;
		rgb |= ((u32)F32toInt8(result.y)) << BitOffsets::G;
		rgb |= ((u32)F32toInt8(result.z)) << BitOffsets::B;
		rgb |= ((u32)F32toInt8(result.w)) << BitOffsets::A;
	}

	constexpr ColorU32& operator= (HexCodeString inHexCode) {
		auto result = fromHexCode(inHexCode.get());
		SetComponent(BitOffsets::R, result.x);
		SetComponent(BitOffsets::G, result.y);
		SetComponent(BitOffsets::B, result.z);
		SetComponent(BitOffsets::A, result.w);
		return *this;
	}

	// Doesn't mutate the object
	// Helper to multiply alpa by a value
	constexpr ColorU32 MultiplyAlpha(float inValue) const {
		auto out = *this;
		out.SetComponent(BitOffsets::A, out.GetComponentAsFloat(BitOffsets::A) * Saturate(inValue));
		return out;
	}

	// Returns true is Alpa is 0
	constexpr bool IsTransparent() const {
		return GetComponentAsFloat(A) == 0.f;
	}

	constexpr operator u32() const { return rgb; }

private:

	constexpr float GetComponentAsFloat(BitOffsets inComponent) const {
		return (float)((rgb >> inComponent) & (u8)0xff) / 255.f;
	}

	constexpr void SetComponent(BitOffsets inComponent, float inValue) {
		rgb &= ~((u8)0xff << inComponent);
		rgb |= ((u32)F32toInt8(inValue)) << inComponent;
	}

private:
	u32 rgb;
};

/**
 * RGB color with 4 float values
 */
struct ColorFloat4 {

	constexpr ColorFloat4()
		: r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}

	constexpr ColorFloat4(float inVal)
		: r(Math::Clamp(inVal, 0.f, 1.f)), g(Math::Clamp(inVal, 0.f, 1.f)), b(Math::Clamp(inVal, 0.f, 1.f)), a(1.0f) {}

	constexpr ColorFloat4(float inR, float inG, float inB, float inA = 1.f)
		: r(Math::Clamp(inR, 0.f, 1.f)), g(Math::Clamp(inG, 0.f, 1.f)), b(Math::Clamp(inB, 0.f, 1.f)), a(Math::Clamp(inA, 0.f, 1.f)) {}

	constexpr ColorFloat4(const Vec4& inFloat4)
		: r(Math::Clamp(inFloat4.x, 0.f, 1.f)), g(Math::Clamp(inFloat4.y, 0.f, 1.f)), b(Math::Clamp(inFloat4.z, 0.f, 1.f)), a(Math::Clamp(inFloat4.w, 0.f, 1.f)) {}

	constexpr ColorFloat4(std::string_view inHexColor)
		: r(0.0f), g(0.0f), b(0.0f), a(1.0f) {
		auto result = fromHexCode(inHexColor);
		r = result.x;
		g = result.y;
		b = result.z;
		a = result.w;
	}

	constexpr operator Vec4() const { return Vec4(r, g, b, a); }

	float r;
	float g;
	float b;
	float a;
};

namespace Util {

	struct Sides {
		u16		Left;
		u16		Right;
		u16		Top;
		u16		Bottom;

		constexpr Sides(u16 val = 0) {
			Left = val;
			Right = val;
			Top = val;
			Bottom = val;
		}

		constexpr Sides(u16 horizontal, u16 vertical) {
			Left = horizontal;
			Right = horizontal;
			Top = vertical;
			Bottom = vertical;
		}

		// TRouBLe
		constexpr Sides(u16 top, u16 right, u16 bottom, u16 left) {
			Left = left;
			Right = right;
			Top = top;
			Bottom = bottom;
		}

		constexpr u16 Vertical() const {
			return Top + Bottom;
		}

		constexpr u16 Horizontal() const {
			return Left + Right;
		}

		constexpr u16& operator[](Side inSide) {
			switch(inSide) {
				case SideTop:	return Top;
				case SideBottom:return Bottom;
				case SideLeft:	return Left;
				case SideRight:	return Right;
			}
			return Left;
		}

		constexpr u16 operator[](Side inSide) const {
			switch(inSide) {
				case SideTop:	return Top;
				case SideBottom:return Bottom;
				case SideLeft:	return Left;
				case SideRight:	return Right;
			}
			return Left;
		}

		constexpr operator Vec4() const {
			return {(float)Top, (float)Right, (float)Bottom, (float)Left};
		}
	};

	inline std::wstring ToWideString(const std::string& inStr) {
		auto out = std::wstring(inStr.size() + 1, L'\0');
		size_t convertedChars = 0;
		mbstowcs_s(&convertedChars, out.data(), out.size(), inStr.c_str(), _TRUNCATE);
		return out;
	}
}