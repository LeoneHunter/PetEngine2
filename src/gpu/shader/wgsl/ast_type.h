#pragma once
#include "ast_node.h"
#include "base/string_utils.h"

namespace wgsl::ast {

// Type info node
//   Builtin types:
//     Primitime types: i32, u32, f32
//     Type generators: vec2<T>, vec3<T>, array<T, N>, atomic<T>
//   User types:
//     Structs: struct Foo {};  struct Bar {};
//     Aliases: alias Foo = Bar;
class Type : public Node {
public:
    enum class Kind {
        AbstrInt,
        AbstrFloat,
        // Concrete
        Bool,
        U32,
        I32,
        F32,
        F16,
        // Templated
        Array,
        Vec2,
        Vec3,
        Vec4,
        Mat,
        // User defined
        Struct,
        Alias,
    };

    Kind kind;
    // full mangled name: array<array<u32, 10>, 10>
    std::string_view name;
    // Declaration for custom user defined types [alias | struct]
    ast::Node* customDecl = nullptr;

public:
    constexpr static inline auto kStaticType = NodeType::Type;

    Type(SourceLoc loc, Kind kind, std::string_view name)
        : Node(loc, kStaticType), kind(kind), name(name) {}

    constexpr bool IsArithmetic() const { return IsInteger() || IsFloat(); }

    constexpr bool IsAbstract() const {
        return kind == Kind::AbstrFloat || kind == Kind::AbstrInt;
    }

    constexpr bool IsSigned() const { return kind != Kind::U32; }

    constexpr bool IsBool() const { return kind == Kind::Bool; }

    // TODO: Handle templates
    constexpr bool IsInteger() const {
        switch (kind) {
            case Kind::U32:
            case Kind::I32:
            case Kind::AbstrInt: return true;
            default: return false;
        }
    }

    // TODO: Handle templates
    constexpr bool IsFloat() const {
        switch (kind) {
            case Kind::F32:
            case Kind::F16:
            case Kind::AbstrFloat: return true;
            default: return false;
        }
    }

    constexpr bool AutoConvertibleTo(const ast::Type* other) {
        if (this == other) {
            return true;
        }
        if (!IsAbstract()) {
            return kind == other->kind;
        }
        if (kind == Kind::AbstrFloat) {
            return other->kind == Kind::F32 || other->kind == Kind::F16;
        }
        // Abstract int is convertible to all other types
        return true;
    }

    constexpr uint32_t GetConversionRankTo(const ast::Type* other) {
        constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
        if (this == other) {
            return 0;
        }
        // Automatic conversion table for abstract types
        // https://www.w3.org/TR/WGSL/#conversion-rank
        if (kind == Kind::AbstrFloat) {
            switch (other->kind) {
                case Kind::F32: return 1;
                case Kind::F16: return 2;
                default: return kMax;
            }
        }
        if (kind == Kind::AbstrInt) {
            switch (other->kind) {
                case Kind::I32: return 3;
                case Kind::U32: return 4;
                case Kind::AbstrFloat: return 5;
                case Kind::F32: return 6;
                case Kind::F16: return 7;
                default: return kMax;
            }
        }
        // Concrete types are not convertible
        return kMax;
    }
};

constexpr std::string_view to_string(Type::Kind kind) {
    switch (kind) {
        case Type::Kind::AbstrInt: return "abstr_int";
        case Type::Kind::AbstrFloat: return "abstr_float";
        case Type::Kind::Bool: return "bool";
        case Type::Kind::U32: return "u32";
        case Type::Kind::I32: return "i32";
        case Type::Kind::F32: return "f32";
        case Type::Kind::F16: return "f16";
        case Type::Kind::Array: return "array";
        case Type::Kind::Vec2: return "vec2";
        case Type::Kind::Vec3: return "vec3";
        case Type::Kind::Vec4: return "vec4";
        case Type::Kind::Mat: return "mat";
        case Type::Kind::Struct: return "struct";
        case Type::Kind::Alias: return "alias";
        default: return "";
    }
}

constexpr std::string_view GetTypeSymbolName(Type::Kind kind) {
    return to_string(kind);
}


}  // namespace wgsl::ast

DEFINE_FORMATTER(wgsl::ast::Type::Kind, std::string_view, to_string);