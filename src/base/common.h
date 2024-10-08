#pragma once
#include "rtti.h"
#include "error.h"
#include "log.h"

#include <span>

#if defined(IS_DEBUG_BUILD)
    constexpr bool kDebugBuild = true;
#else
    constexpr bool kDebugBuild = false;
#endif

#define NONCOPYABLE(TypeName) \
    TypeName(TypeName&&) = delete; \
    TypeName(const TypeName&) = delete;

class NonCopyable {
protected:
    constexpr NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;

private:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

#define NODISCARD [[nodiscard]]

#define NOT_IMPLEMENTED() DASSERT_M(false, "Not implemented!");

// Defines all bitwise operators for enum classes so it can be (mostly) used as a regular flags enum
#define DEFINE_ENUM_FLAGS_OPERATORS(Enum) \
    inline constexpr Enum& operator|=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((std::underlying_type_t<Enum>)Lhs | (std::underlying_type_t<Enum>)Rhs); }	\
    inline constexpr Enum& operator&=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((std::underlying_type_t<Enum>)Lhs & (std::underlying_type_t<Enum>)Rhs); }	\
    inline constexpr Enum& operator^=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((std::underlying_type_t<Enum>)Lhs ^ (std::underlying_type_t<Enum>)Rhs); }	\
    inline constexpr Enum  operator| (Enum  Lhs, Enum Rhs) { return (Enum)((std::underlying_type_t<Enum>)Lhs | (std::underlying_type_t<Enum>)Rhs); }		\
    inline constexpr bool  operator& (Enum  Lhs, Enum Rhs) { return (bool)((std::underlying_type_t<Enum>)Lhs & (std::underlying_type_t<Enum>)Rhs); }		\
    inline constexpr Enum  operator^ (Enum  Lhs, Enum Rhs) { return (Enum)((std::underlying_type_t<Enum>)Lhs ^ (std::underlying_type_t<Enum>)Rhs); }		\
    inline constexpr bool  operator! (Enum  E)             { return !(std::underlying_type_t<Enum>)E; }														\
    inline constexpr Enum  operator~ (Enum  E)             { return (Enum)~(std::underlying_type_t<Enum>)E; }

#define DEFINE_ENUM_FLAGS_OPERATORS_FRIEND(Enum) \
friend	inline constexpr Enum& operator|=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((std::underlying_type_t<Enum>)Lhs | (std::underlying_type_t<Enum>)Rhs); }	\
friend	inline constexpr Enum& operator&=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((std::underlying_type_t<Enum>)Lhs & (std::underlying_type_t<Enum>)Rhs); }	\
friend	inline constexpr Enum& operator^=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((std::underlying_type_t<Enum>)Lhs ^ (std::underlying_type_t<Enum>)Rhs); }	\
friend	inline constexpr Enum  operator| (Enum  Lhs, Enum Rhs) { return (Enum)((std::underlying_type_t<Enum>)Lhs | (std::underlying_type_t<Enum>)Rhs); }		\
friend	inline constexpr bool  operator& (Enum  Lhs, Enum Rhs) { return (bool)((std::underlying_type_t<Enum>)Lhs & (std::underlying_type_t<Enum>)Rhs); }		\
friend	inline constexpr Enum  operator^ (Enum  Lhs, Enum Rhs) { return (Enum)((std::underlying_type_t<Enum>)Lhs ^ (std::underlying_type_t<Enum>)Rhs); }		\
friend	inline constexpr bool  operator! (Enum  E)             { return !(std::underlying_type_t<Enum>)E; }														\
friend	inline constexpr Enum  operator~ (Enum  E)             { return (Enum)~(std::underlying_type_t<Enum>)E; }

#define DEFINE_ENUMFLAGS_ENTRY(type, entry) if(inEnum & type::entry) { out.empty() ? out.append(#entry) : out.append(" | "#entry); }

#define DEFINE_ENUMFLAGS_TOSTRING_2(type, deflt, enum1)\
    constexpr std::string ToString(type inEnum) {\
        std::string out;\
        DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
        if(out.empty()) out = #deflt;\
        return out;\
    }

#define DEFINE_ENUMFLAGS_TOSTRING_3(type, deflt, enum1, enum2)\
    constexpr std::string ToString(type inEnum) {\
        std::string out;\
        DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
        if(out.empty()) out = #deflt;\
        return out;\
    }

#define DEFINE_ENUMFLAGS_TOSTRING_4(type, deflt, enum1, enum2, enum3)\
    constexpr std::string ToString(type inEnum) {\
        std::string out;\
        DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum3)\
        if(out.empty()) out = #deflt;\
        return out;\
    }

#define DEFINE_ENUMFLAGS_TOSTRING_5(type, deflt, enum1, enum2, enum3, enum4)\
    constexpr std::string ToString(type inEnum) {\
        std::string out;\
        DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum3)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum4)\
        if(out.empty()) out = #deflt;\
        return out;\
    }

#define DEFINE_ENUMFLAGS_TOSTRING_6(type, deflt, enum1, enum2, enum3, enum4, enum5)\
    constexpr std::string ToString(type inEnum) {\
        std::string out;\
        DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum3)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum4)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum5)\
        if(out.empty()) out = #deflt;\
        return out;\
    }

#define DEFINE_ENUMFLAGS_TOSTRING_7(type, deflt, enum1, enum2, enum3, enum4, enum5, enum6)\
    constexpr std::string ToString(type inEnum) {\
        std::string out;\
        DEFINE_ENUMFLAGS_ENTRY(type, enum1)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum2)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum3)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum4)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum5)\
        DEFINE_ENUMFLAGS_ENTRY(type, enum6)\
        if(out.empty()) out = #deflt;\
        return out;\
    }



#define DEFINE_ENUM_TOSTRING_1(type, e1)\
    constexpr std::string ToString(type inEnum) {\
        if(inEnum == e1) { return #e1; }\
        return {};\
    }

#define DEFINE_ENUM_TOSTRING_2(type, e1, e2)\
    constexpr std::string ToString(type inEnum) {\
        if(inEnum == type::e1) { return #e1; }\
        else if(inEnum == type::e2) { return #e2; }\
        return {};\
    }

#define DEFINE_ENUM_TOSTRING_3(type, e1, e2, e3)\
    constexpr std::string ToString(type inEnum) {\
        if(inEnum == type::e1) { return #e1; }\
        else if(inEnum == type::e2) { return #e2; }\
        else if(inEnum == type::e3) { return #e3; }\
        return {};\
    }

#define DEFINE_ENUM_TOSTRING_4(type, e1, e2, e3, e4)\
    constexpr std::string ToString(type inEnum) {\
        if(inEnum == type::e1) { return #e1; }\
        else if(inEnum == type::e2) { return #e2; }\
        else if(inEnum == type::e3) { return #e3; }\
        else if(inEnum == type::e4) { return #e4; }\
        return {};\
    }

#define DEFINE_ENUM_TOSTRING_5(type, e1, e2, e3, e4, e5)\
    constexpr std::string ToString(type inEnum) {\
        if(inEnum == type::e1) { return #e1; }\
        else if(inEnum == type::e2) { return #e2; }\
        else if(inEnum == type::e3) { return #e3; }\
        else if(inEnum == type::e4) { return #e4; }\
        else if(inEnum == type::e5) { return #e5; }\
        return {};\
    }


template<class T>
concept HasToString = requires (const T& val) { 
    { to_string(val) } -> std::convertible_to<std::string>;
};

template<HasToString T>
struct std::formatter<T, char> {	

    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<class FmtContext>
    FmtContext::iterator format(const T& val, FmtContext& ctx) const {
        return std::ranges::copy(to_string(val), ctx.out()).out;
    }
};


// Defines a range of elements of type V
// std::vector<V>, std::array<3, V>, std::string<V>
template <class T, class V>
concept Range =
    std::ranges::range<T> && std::convertible_to<typename T::value_type, V>;

template <class T, size_t Size>
constexpr size_t SizeBytes(T (&arr)[Size]) {
    return Size * sizeof(T);
}

template <std::ranges::contiguous_range Range>
constexpr size_t SizeBytes(Range&& range) {
    using value_type = std::remove_cvref_t<decltype(*std::begin(range))>;
    return std::size(range) * sizeof(value_type);
}



using ByteSpan = std::span<const uint8_t>;

template <class T, size_t Size>
constexpr ByteSpan AsByteSpan(T (&arr)[Size]) {
    return {(const uint8_t*)arr, Size * sizeof(T)};
}

template <std::ranges::contiguous_range Range>
constexpr ByteSpan AsByteSpan(const Range& container) {
    const auto size = std::size(container);
    const auto begin = std::begin(container);
    const auto* ptr = (const uint8_t*)&*begin;
    return {ptr, size};
}