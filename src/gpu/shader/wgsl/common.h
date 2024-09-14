#pragma once
#include "base/common.h"
#include "base/string_utils.h"

namespace wgsl {

// Error code and default string description
#define ERROR_CODES(V)                                                         \
    /* Generic errors */                                                       \
    V(Ok, "ok")                                                                \
    V(Unmatched, "expected token not matched")                                 \
    /* Syntax errors */                                                        \
    V(Unimplemented, "feature is not implemented")                             \
    V(UnexpectedToken, "unexpected token")                                     \
    V(ExpectedLiteral, "expected a literal")                                   \
    V(ExpectedExpr, "expected an expresssion")                                 \
    V(ExpectedType, "expected a type specifier")                               \
    V(ExpectedIdent, "expected an identifier")                                 \
    V(ExpectedDecl, "expected a declaration")                                  \
    V(InvalidAttribute, "invalid attribute name")                              \
    V(ConstNoAttr, "const value may not have attributes")                      \
    V(IdentReserved, "identifier is reserved")                                 \
    V(ExpectedStorageClass, "const, var, override or let is expected")         \
    V(LiteralInitValueTooLarge, "initializer value too large")                 \
    V(ExpectedStatement, "expected a statement")                               \
    /* Semantic errors */                                                      \
    V(InvalidVarBinding, "this var declaration may not have such type")        \
    V(InvalidScope, "this declaration cannot appear in the current scope")     \
    V(EmptyStruct, "a struct declaration must have members specified")         \
    V(DuplicateMember, "member redefinition")                                  \
    V(ConstDeclNoInitializer,                                                  \
      "unexpected token. const value requires an initializer")                 \
    V(TypeNotDefined, "type is not defined")                                   \
    V(IdentNotType, "identifier is not a type name")                           \
    V(TypeError, "expression cannot be assigned to the value of type")         \
    V(SymbolAlreadyDefined, "symbol already defined in the current scope")     \
    V(SymbolNotFound, "symbol not found")                                      \
    V(SymbolNotVariable, "symbol is not a variable")                           \
    V(GlobalVarInitializer, "this var declaration may not have initializers")  \
    V(InvalidTemplateParam, "unexpected template parameter")                   \
    V(InvalidAddrSpace,                                                        \
      "a variable with this address space may not be declared in the current " \
      "scope")                                                                 \
    V(InvalidArg, "no operator matches the arguments")                         \
    V(ConstOverflow,                                                           \
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

template <class T>
using Expected = std::expected<T, ErrorCode>;



//=====================================================//
// Variable address space
//=====================================================//
#define ADDRESS_SPACE_LIST(V) \
    V(Function, "function")   \
    V(Private, "private")     \
    V(Workgroup, "workgroup") \
    V(Uniform, "uniform")     \
    V(Storage, "storage")     \
    V(Handle, "handle")

#define ENUM(NAME, STR) NAME,
enum class AddressSpace { ADDRESS_SPACE_LIST(ENUM) };
#undef ENUM

constexpr std::optional<AddressSpace> AddressSpaceFromString(
    std::string_view str) {
#define CASE(NAME, STR)            \
    if (str == STR)                \
        return AddressSpace::NAME; \
    else
    ADDRESS_SPACE_LIST(CASE)
    return std::nullopt;
#undef CASE
}

constexpr std::string_view to_string(AddressSpace as) {
#define CASE(NAME, STR) \
    case AddressSpace::NAME: return STR;
    switch (as) {
        ADDRESS_SPACE_LIST(CASE)
        default: return "";
    }
#undef CASE
}



//=====================================================//
// Variable access mode
//=====================================================//
#define ACCESS_MODE_LIST(V) \
    V(Read, "read")         \
    V(Write, "write")       \
    V(ReadWrite, "read_write")

#define ENUM(NAME, STR) NAME,
enum class AccessMode { ACCESS_MODE_LIST(ENUM) };
#undef ENUM

constexpr std::optional<AccessMode> AccessModeFromString(std::string_view str) {
#define CASE(NAME, STR)          \
    if (str == STR)              \
        return AccessMode::NAME; \
    else
    ACCESS_MODE_LIST(CASE)
    return std::nullopt;
#undef CASE
}

constexpr std::string_view to_string(AccessMode as) {
#define CASE(NAME, STR) \
    case AccessMode::NAME: return STR;
    switch (as) {
        ACCESS_MODE_LIST(CASE)
        default: return "";
    }
#undef CASE
}



//=====================================================//
// Attributes
//=====================================================//
#define ATTRIBUTE_LIST(V)              \
    V(Align, "align")                  \
    V(Binding, "binding")              \
    V(BlendSrc, "blend_src")           \
    V(Builtin, "builtin")              \
    V(Const, "const")                  \
    V(Diagnostic, "diagnostic")        \
    V(Group, "group")                  \
    V(ID, "id")                        \
    V(Interpolate, "interpolate")      \
    V(Invariant, "invariant")          \
    V(Location, "location")            \
    V(MustUse, "must_use")             \
    V(Size, "size")                    \
    V(WorkgroupSize, "workgroup_size") \
    V(Vertex, "vertex")                \
    V(Fragment, "gragment")            \
    V(Compute, "compute")

#define ENUM(NAME, STR) NAME,
enum class AttributeName { ATTRIBUTE_LIST(ENUM) };
#undef ENUM

constexpr std::optional<AttributeName> AttributeFromString(
    std::string_view str) {
#define CASE(NAME, STR)             \
    if (str == STR)                 \
        return AttributeName::NAME; \
    else
    ATTRIBUTE_LIST(CASE)
    return std::nullopt;
#undef CASE
}

constexpr std::string_view to_string(AttributeName attr) {
#define CASE(NAME, STR) \
    case AttributeName::NAME: return STR;
    switch (attr) {
        ATTRIBUTE_LIST(CASE)
        default: return "";
    }
#undef CASE
}



//=====================================================//
// Builtins
//=====================================================//
#define BUILTINS_LIST(V)                              \
    V(VertexIndex, "vertex_index")                    \
    V(InstanceIndex, "instance_index")                \
    V(Position, "position")                           \
    V(FrontFacing, "front_facing")                    \
    V(FragDepth, "frag_depth")                        \
    V(SampleIndex, "sample_index")                    \
    V(SampleMask, "sample_mask")                      \
    V(LocalInvocationID, "local_invocation_id")       \
    V(LocalInvocationIndex, "local_invocation_index") \
    V(GlobalInvocationID, "global_invocation_id")     \
    V(WorkGroupID, "workgroup_id")                    \
    V(NumWorkgroups, "num_workgroups")

constexpr std::string_view kBuiltinValues[] = {
#define V(NAME, STR) STR,
    BUILTINS_LIST(V)
#undef V
};

enum class Builtin {
#define ENUM(NAME, STR) NAME,
    BUILTINS_LIST(ENUM)
#undef ENUM
};

constexpr std::optional<Builtin> BuiltinFromString(std::string_view str) {
#define CASE(NAME, STR)       \
    if (str == STR)           \
        return Builtin::NAME; \
    else
    BUILTINS_LIST(CASE)
    return std::nullopt;
#undef CASE
}



//=====================================================//
// Keywords
//=====================================================//
#define KEYWORD_LIST(V)            \
    V(Alias, "alias")              \
    V(Break, "break")              \
    V(Case, "case")                \
    V(Const, "const")              \
    V(ConstAssert, "const_assert") \
    V(Continue, "continue")        \
    V(Continuing, "continuing")    \
    V(Default, "default")          \
    V(Diagnostic, "diagnostic")    \
    V(Discard, "discard")          \
    V(Else, "else")                \
    V(Enable, "enable")            \
    V(False, "false")              \
    V(Fn, "fn")                    \
    V(For, "for")                  \
    V(If, "if")                    \
    V(Let, "let")                  \
    V(Loop, "loop")                \
    V(Override, "override")        \
    V(Requires, "requires")        \
    V(Return, "return")            \
    V(Struct, "struct")            \
    V(Switch, "switch")            \
    V(True, "true")                \
    V(Var, "var")                  \
    V(While, "while")


constexpr std::string_view kKeywords[] = {
#define V(NAME, STR) STR,
    KEYWORD_LIST(V)
#undef V
};

enum class Keyword {
#define ENUM(NAME, STR) NAME,
    KEYWORD_LIST(ENUM)
#undef ENUM
};

constexpr std::string_view to_string(Keyword keyword) {
#define CASE(NAME, STR) \
    case Keyword::NAME: return STR;
    switch (keyword) {
        KEYWORD_LIST(CASE)
        default: return "";
    }
#undef CASE
}

constexpr std::optional<Keyword> KeywordFromString(std::string_view str) {
#define CASE(NAME, STR)       \
    if (str == STR)           \
        return Keyword::NAME; \
    else
    KEYWORD_LIST(CASE)
    return std::nullopt;
#undef CASE
}



// Reserved identifiers for future use
// clang-format off
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
                len = rhs.col + rhs.len - lhs.col;
            } else {
                len = lhs.col + rhs.len - rhs.col;
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


}  // namespace wgsl