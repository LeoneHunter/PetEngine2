#pragma once
#include "runtime/common.h"
#include "runtime/string_utils.h"

namespace gpu {

// Index into the root signature for shader
// Basically an index of a shader parameter
struct BindIndex {
    constexpr BindIndex(uint32_t val): value(val) {}
    constexpr auto operator<=>(const BindIndex& rhs) const = default;
    constexpr explicit operator uint32_t() const { return value; };
    uint32_t value;
};

constexpr BindIndex kInvalidBindIndex{std::numeric_limits<uint32_t>::max()};

} // namespace gpu

DEFINE_CAST_FORMATTER(gpu::BindIndex, uint32_t);