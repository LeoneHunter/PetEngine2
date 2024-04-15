#pragma once
#include <string>
#include <assert.h>
#include <chrono>

#include "types.h"
#include "math_util.h"


constexpr Vec4 HashColorToVec4(std::string_view inHexCode) {
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

	constexpr static ColorU32 FromHex(std::string_view inStr) {
		auto vec = HashColorToVec4(inStr);
		return {vec.x, vec.y, vec.z, vec.w};
	}

	constexpr ColorU32(float R, float G, float B, float A): rgb() {
		rgb = ((u32)F32toInt8(R)) << BitOffsets::R;
		rgb |= ((u32)F32toInt8(G)) << BitOffsets::G;
		rgb |= ((u32)F32toInt8(B)) << BitOffsets::B;
		rgb |= ((u32)F32toInt8(A)) << BitOffsets::A;
	}

	constexpr ColorU32(): rgb(0xff) {}

	constexpr ColorU32& operator= (std::string_view inHexCode) {
		auto result = HashColorToVec4(inHexCode);
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

	constexpr float GetComponentAsFloat(BitOffsets inComponent) const {
		return (float)((rgb >> inComponent) & (u8)0xff) / 255.f;
	}

	constexpr void SetComponent(BitOffsets inComponent, float inValue) {
		rgb &= ~((u8)0xff << inComponent);
		rgb |= ((u32)F32toInt8(inValue)) << inComponent;
	}

	constexpr operator u32() const { return rgb; }

private:
	u32 rgb;
};

/**
 * RGB color with 4 float values
 */
struct ColorFloat4 {

	constexpr ColorFloat4()
		: r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}

	constexpr ColorFloat4(ColorU32 inColor)
		: r(inColor.GetComponentAsFloat(ColorU32::R))
		, g(inColor.GetComponentAsFloat(ColorU32::G))
		, b(inColor.GetComponentAsFloat(ColorU32::B))
		, a(inColor.GetComponentAsFloat(ColorU32::A)) {}

	constexpr ColorFloat4(float inVal)
		: r(math::Clamp(inVal, 0.f, 1.f)), g(math::Clamp(inVal, 0.f, 1.f)), b(math::Clamp(inVal, 0.f, 1.f)), a(1.0f) {}

	constexpr ColorFloat4(float inR, float inG, float inB, float inA = 1.f)
		: r(math::Clamp(inR, 0.f, 1.f)), g(math::Clamp(inG, 0.f, 1.f)), b(math::Clamp(inB, 0.f, 1.f)), a(math::Clamp(inA, 0.f, 1.f)) {}

	constexpr ColorFloat4(const Vec4& inFloat4)
		: r(math::Clamp(inFloat4.x, 0.f, 1.f)), g(math::Clamp(inFloat4.y, 0.f, 1.f)), b(math::Clamp(inFloat4.z, 0.f, 1.f)), a(math::Clamp(inFloat4.w, 0.f, 1.f)) {}

	constexpr ColorFloat4(std::string_view inHexColor)
		: r(0.0f), g(0.0f), b(0.0f), a(1.0f) {
		auto result = HashColorToVec4(inHexColor);
		r = result.x;
		g = result.y;
		b = result.z;
		a = result.w;
	}

	constexpr explicit operator ColorU32() const { return {r, g, b, a}; }

	float r;
	float g;
	float b;
	float a;
};

namespace util {

	/*
	* Simple timer
	* Wrapper around std::chrono
	*/
	template<typename T = std::chrono::milliseconds>
	struct Timer {

		using value_type = std::chrono::milliseconds;

		Timer()
			: SetPointDuration(0)
			, StartTimePoint()
		{}
				
		void Reset(u32 inValue) {
			SetPointDuration = value_type(inValue);
			StartTimePoint = std::chrono::high_resolution_clock::now();
		}

		bool IsTicking() const { return SetPointDuration.count() != 0; }

		bool IsReady() const { 
			return IsTicking() ? 
				std::chrono::duration_cast<value_type>(std::chrono::high_resolution_clock::now() - StartTimePoint) >= SetPointDuration : 
				false;
		}

		void Clear() { SetPointDuration = value_type(0); }

		value_type SetPointDuration;
		std::chrono::high_resolution_clock::time_point StartTimePoint;
	};


	/*
	* Helper to create string of serialized data
	*/
	class StringBuilder {
	public:

		StringBuilder(std::string* inBuffer, u32 inIndentSize = 2)
			: buffer_(inBuffer)
			, indent_(0)
			, indentSize_(inIndentSize)
		{}

		template <typename... ArgTypes>
		StringBuilder& Line(const std::format_string<ArgTypes...> inFormat, ArgTypes&&... inArgs) {
			AppendIndent();
			buffer_->append(std::format(inFormat, std::forward<ArgTypes>(inArgs)...));
			EndLine();
			return *this;
		}

		StringBuilder& Line(std::string_view inStr) {
			AppendIndent();
			buffer_->append(inStr);
			EndLine();
			return *this;
		}

		StringBuilder& Line() {
			EndLine();
			return *this;
		}

		StringBuilder& SetIndent(u32 inIndent = 1) {
			indent_ = inIndent;
			return *this;
		}

		StringBuilder& PushIndent(u32 inIndent = 1) {
			indent_ += inIndent;
			return *this;
		}

		StringBuilder& PopIndent(u32 inIndent = 1) {
			if(indent_) indent_ -= inIndent;
			return *this;
		}

		StringBuilder& EndLine() {
			buffer_->append("\n");
			return *this;
		}

	private:

		void AppendIndent() {
			if(!indent_) return;
			for(auto i = indent_ * indentSize_; i; --i) {
				buffer_->append(" ");
			}
		}

	private:
		u32			 indentSize_;
		u32			 indent_;
		std::string* buffer_;
	};

	inline std::wstring ToWideString(const std::string& inStr) {
		auto out = std::wstring(inStr.size() + 1, L'\0');
		size_t convertedChars = 0;
		mbstowcs_s(&convertedChars, out.data(), out.size(), inStr.c_str(), _TRUNCATE);
		return out;
	}

	inline std::string ToLower(const std::string& inString) {
		std::string str(inString);
		std::transform(
			std::begin(str),
			std::end(str),
			std::begin(str),
			[](unsigned char c) {
				return std::tolower(c);
			}
		);
		return str;
	}



	class RefCounter {
	public:

		constexpr RefCounter() = default;

		RefCounter(const RefCounter&) = delete;
		RefCounter& operator=(const RefCounter&) = delete;

		void IncWref() {
			++Weaks;
		}

		void DecWref() {
			--Weaks;

			if(Weaks == 0 && bDestructed) {
				delete this;
			}
		}

		void OnDestructed() {
			bDestructed = true;

			if(Weaks == 0) {
				delete this;
			}
		}

		bool IsAlive() const {
			return !bDestructed;
		}

		u64  Weaks = 0;
		bool bDestructed = false;
	};

	
	/*
	* A RAII object that has a shared RefCounter to a controlled object
	* When object is deleted a RefCounter is notified
	* Doesn't own the controlled object
	*/
	template<typename T>
	class WeakPtr {
	public:

		constexpr WeakPtr() {}

		constexpr WeakPtr(nullptr_t) {}

		WeakPtr(T* inPtr, RefCounter* inRC) {
			Ptr = inPtr;
			RC = inRC;
			RC->IncWref();
		}

		WeakPtr(const WeakPtr& inRight) {
			if(inRight.RC && inRight.RC->IsAlive()) {
				Ptr = inRight.Ptr;
				RC = inRight.RC;
				RC->IncWref();
			}
		}

		WeakPtr(WeakPtr&& inRight) {
			if(inRight.RC && inRight.RC->IsAlive()) {
				Ptr = inRight.Ptr;
				RC = inRight.RC;
			}
			inRight.Ptr = nullptr;
			inRight.RC = nullptr;
		}

		~WeakPtr() {
			if(RC) {
				RC->DecWref();
			}
		}

		WeakPtr& operator=(const WeakPtr& inRight) {
			if(inRight.RC && inRight.RC->IsAlive()) {
				Reset();
				Ptr = inRight.Ptr;
				RC = inRight.RC;
				RC->IncWref();
			}
			return *this;
		}

		WeakPtr& operator=(WeakPtr&& inRight) noexcept {
			Reset();
			if(inRight.RC && inRight.RC->IsAlive()) {
				Ptr = inRight.Ptr;
				RC = inRight.RC;
			}
			inRight.Ptr = nullptr;
			inRight.RC = nullptr;
			return *this;
		}

		WeakPtr& operator=(nullptr_t) noexcept {
			Reset();
			return *this;
		}

		void Reset() {
			if(RC) {
				RC->DecWref();
			}
			RC = nullptr;
			Ptr = nullptr;
		}

		explicit operator bool() const {
			return RC && RC->IsAlive();
		}

		T* operator->() { return Ptr; }
		const T* operator->() const { return Ptr; }

		T* operator*() { return Ptr; }
		const T* operator*() const { return Ptr; }

		T* Get() { return Ptr; }
		const T* Get() const { return Ptr; }

		T* GetChecked() { return RC ? RC->IsAlive() ? Ptr : nullptr : nullptr; }
		const T* GetChecked() const { return RC ? RC->IsAlive() ? Ptr : nullptr : nullptr; }

		bool Expired() const { return !RC || !RC->IsAlive(); }

	private:
		RefCounter* RC  = nullptr;
		T*			Ptr = nullptr;
	};

	template<class T1, class T2>
	constexpr bool operator==(const WeakPtr<T1>& lhs, const WeakPtr<T2>& rhs) {
		return (uintptr_t)lhs.Get() == (uintptr_t)rhs.Get();
	}

	template<class T>
	constexpr bool operator==(const WeakPtr<T>& lhs, const T* inPtr) {
		return lhs.Get() == inPtr;
	}
}