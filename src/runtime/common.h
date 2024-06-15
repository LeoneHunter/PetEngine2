#pragma once
#include "rtti.h"
#include "error.h"
#include "log.h"

#ifndef NDEBUG
	constexpr bool kDebugBuild = true;
#else
	constexpr bool kDebugBuild = false;
#endif

#define NONCOPYABLE(TypeName) \
	TypeName(TypeName&&) = delete; \
	TypeName(const TypeName&) = delete;

#define NODISCARD [[nodiscard]]

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