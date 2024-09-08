#pragma once
#include "base/common.h"
#include "base/string_utils.h"

namespace wgsl {

// Error code and default string description
#define ERROR_CODES(V)                                                     \
    /* Syntax errors */                                                    \
    V(Unimplemented, "feature is not implemented")                         \
    V(UnexpectedToken, "unexpected token")                                 \
    V(ExpectedExpr, "expected an expresssion")                             \
    V(ExpectedType, "expected a type specifier")                           \
    V(ExpectedIdent, "expected an identifier")                             \
    V(ExpectedDecl, "expected a declaration")                              \
    V(InvalidAttribute, "invalid attribute name")                          \
    V(ConstNoAttr, "const value may not have attributes")                  \
    V(IdentReserved, "identifier is reserved")                             \
    V(ExpectedStorageClass, "const, var, override or let is expected")     \
    V(LiteralInitValueTooLarge, "initializer value too large")             \
    /* Semantic errors */                                                  \
    V(ConstDeclNoInitializer,                                              \
      "unexpected token. const value requires an initializer")             \
    V(TypeNotDefined, "type is not defined")                               \
    V(IdentNotType, "identifier is not a type name")                       \
    V(TypeError, "expression cannot be assigned to the value of type")     \
    V(SymbolAlreadyDefined, "symbol already defined in the current scope") \
    V(SymbolNotFound, "symbol not found")                                  \
    V(SymbolNotVariable, "symbol is not a variable")                       \
    /* Expression errors */                                                \
    V(InvalidArgs, "no operator matches the arguments")                      \
    V(ConstOverflow,                                                       \
      "this operation cannot result in a constant value. numeric overflow")


// Define enum
#define ENUM(Name, Str) Name,
enum class ErrorCode { ERROR_CODES(ENUM) };
#undef ENUM

constexpr std::string_view ErrorCodeDefaultMsg(ErrorCode code) {
#define CASE(Name, Str) \
    case ErrorCode::Name: return Str;
    switch (code) {
        ERROR_CODES(CASE)
        default: return "";
    }
#undef CASE
}

constexpr std::string_view ErrorCodeString(ErrorCode code) {
#define CASE(Name, Str) \
    case ErrorCode::Name: return #Name;
    switch (code) {
        ERROR_CODES(CASE)
        default: return "";
    }
#undef CASE
}

}  // namespace wgsl

DEFINE_OSTREAM(wgsl::ErrorCode, ErrorCodeString);
DEFINE_FORMATTER(wgsl::ErrorCode, std::string_view, ErrorCodeString);

namespace wgsl {
// Source code location and size
// Basically a std::string_view but with line:col instead of pointer
struct SourceLoc {
    uint32_t line = 1;
    uint32_t col = 1;
    uint32_t len = 1;

    constexpr SourceLoc() = default;

    constexpr SourceLoc(SourceLoc lhs, SourceLoc rhs)
        : line(lhs.line), col(lhs.col), len() {
        if (lhs.line == rhs.line) {
            if (rhs.col > lhs.col) {
                len = rhs.col - lhs.col;
            } else {
                len = lhs.col - rhs.col;
            }
        }
    }

    constexpr SourceLoc(SourceLoc lhs, uint32_t len)
        : line(lhs.line), col(lhs.col), len(len) {}

    constexpr SourceLoc(uint32_t line, uint32_t col, uint32_t len)
        : line(line), col(col), len(len) {}

    // Add column offset
    friend constexpr SourceLoc operator+(SourceLoc lhs, uint32_t offset) {
        return {lhs.line, lhs.col + offset, lhs.len};
    }

    constexpr auto operator<=>(const SourceLoc&) const = default;
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

constexpr std::string_view to_string(AttributeName attr) {
    switch (attr) {
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

}  // namespace wgsl