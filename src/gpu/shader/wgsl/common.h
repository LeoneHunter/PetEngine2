#pragma once
#include "base/common.h"

namespace wgsl {

// Source code location
struct Location {
    uint32_t line = 0;
    uint32_t col = 1;
};

// node source range, including end: [start, end]
struct LocationRange {
    Location start;
    Location end;

    LocationRange(Location start, Location end = {}) : start(start), end(end) {}
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

enum class Attribute {
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

constexpr std::string to_string(Attribute attr) {
    switch(attr) {
        case Attribute::Align: return "align";
        case Attribute::Binding: return "binding";
        case Attribute::BlendSrc: return "blend_src";
        case Attribute::Builtin: return "builtin";
        case Attribute::Const: return "const";
        case Attribute::Diagnostic: return "diagnostic";
        case Attribute::Group: return "group";
        case Attribute::ID: return "id";
        case Attribute::Interpolate: return "interpolate";
        case Attribute::Invariant: return "invariant";
        case Attribute::Location: return "location";
        case Attribute::MustUse: return "must_use";
        case Attribute::Size: return "size";
        case Attribute::WorkGroupSize: return "workgroup_size";
        case Attribute::Vertex: return "vertex";
        case Attribute::Fragment: return "fragment";
        case Attribute::Compute: return "compute";
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