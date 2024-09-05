#pragma once
#include "base/common.h"

namespace wgsl {

// Source code location
struct Location {
    uint32_t line = 0;
    uint32_t col = 1;

    constexpr Location() = default;

    constexpr Location(uint32_t line, uint32_t col)
        : line(line), col(col) {}

    constexpr auto operator<=>(const Location&) const = default;
};

// node source range, including end: [start, end]
struct LocationRange {
    Location start;
    Location end;

    constexpr LocationRange() = default;

    constexpr LocationRange(Location start, Location end = {}) 
        : start(start), end(end) {
        if(end < start) {
            end = start;
        }
    }

    constexpr auto operator<=>(const LocationRange&) const = default;
};

// clang-format off
constexpr std::string_view kKeywords[] = {
    "alias",    "break",      "case",    "const",      "const_assert",
    "continue", "continuing", "default", "diagnostic", "discard",
    "else",     "enable",     "false",   "fn",         "for",
    "if",       "let",        "loop",    "override",   "requires",
    "return",   "struct",     "switch",  "true",       "var",
    "while",
};

// Reserved identifiers for future use
constexpr std::string_view kReserved[] = {
    "NULL", "Self", "abstract", "active", "alignas", "alignof", "as",
    "asm", "asm_fragment", "async", "attribute", "auto", "await", "become",
    "binding_array", "cast", "catch", "class", "co_await", "co_return", "co_yield",
    "coherent", "column_major", "common", "compile", "compile_fragment", "concept", "const_cast",
    "consteval", "constexpr", "constinit", "crate", "debugger", "decltype", "delete",
    "demote", "demote_to_helper", "do", "dynamic_cast", "enum", "explicit", "export",
    "extends", "extern", "external", "fallthrough", "filter", "final", "finally",
    "friend", "from", "fxgroup", "get", "goto", "groupshared", "highp", "impl",
    "implements", "import", "inline", "instanceof", "interface", "layout", "lowp", "macro",
    "macro_rules", "match", "mediump", "meta", "mod", "module", "move", "mut",
    "mutable", "namespace", "new", "nil", "noexcept", "noinline", "nointerpolation", "noperspective",
    "null", "nullptr", "of", "operator", "package", "packoffset", "partition", "pass",
    "patch", "pixelfragment", "precise", "precision", "premerge", "priv", "protected",
    "pub", "public", "readonly", "ref", "regardless", "register", "reinterpret_cast", "require",
    "resource", "restrict", "self", "set", "shared", "sizeof", "smooth", "snorm",
    "static", "static_assert", "static_cast", "std", "subroutine", "super", "target", "template",
    "this", "thread_local", "throw", "trait", "try", "type", "typedef", "typeid", "typename",
    "typeof", "union", "unless", "unorm", "unsafe", "unsized", "use", "using", "varying",
    "virtual", "volatile", "wgsl", "where", "with", "writeonly", "yield",
};
// clang-format on

enum class AttributeName {
    Align,
    Binding,
    BlendSrc,
    Builtin,
    Const,
    Diagnostic,
    Group, 
    ID,
    Interpolate,
    Invariant,
    Location,
    MustUse,
    Size,
    WorkGroupSize,
    Vertex,
    Fragment,
    Compute,
};

constexpr std::string to_string(AttributeName attr) {
    switch(attr) {
        case AttributeName::Align: return "align";
        case AttributeName::Binding: return "binding";
        case AttributeName::BlendSrc: return "blend_src";
        case AttributeName::Builtin: return "builtin";
        case AttributeName::Const: return "const";
        case AttributeName::Diagnostic: return "diagnostic";
        case AttributeName::Group: return "group";
        case AttributeName::ID: return "id";
        case AttributeName::Interpolate: return "interpolate";
        case AttributeName::Invariant: return "invariant";
        case AttributeName::Location: return "location";
        case AttributeName::MustUse: return "must_use";
        case AttributeName::Size: return "size";
        case AttributeName::WorkGroupSize: return "workgroup_size";
        case AttributeName::Vertex: return "vertex";
        case AttributeName::Fragment: return "fragment";
        case AttributeName::Compute: return "compute";
    }
}

enum class DataType {
    U32,
    I32,
    // Temporary types
    AbsrInt,
    AbstrFloat,
};

} // namespace wgls