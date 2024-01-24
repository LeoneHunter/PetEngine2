#pragma once
#include "stdint.h"

#include <functional>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <format>

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;

using Path = std::filesystem::path;

template<>
struct std::formatter<Path, char> {

	template<class ParseContext>
	constexpr ParseContext::iterator parse(ParseContext& ctx) {
		return ctx.begin();
	}

	template<class FmtContext>
	FmtContext::iterator format(const Path& t, FmtContext& ctx) const {
		return std::ranges::copy(std::move(t.string()), ctx.out()).out;
	}
};

template<typename ...Types>
using VoidFunction = std::function<void(Types...)>;
