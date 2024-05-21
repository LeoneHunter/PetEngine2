#pragma once
#include "types.h"
#include "rtti.h"
#include "error.h"
#include "math_util.h"
#include "serialize.h"

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



class CommandLine {
public:

	static void Set(int argc, char* argv[]) {
		args_ = {argv, argv + argc};
	}

	static std::filesystem::path GetWorkingDir() {
		Assert(!args_.empty());
		return std::filesystem::path(args_.front()).parent_path();
	}

private:
	static inline std::vector<std::string> args_;
};