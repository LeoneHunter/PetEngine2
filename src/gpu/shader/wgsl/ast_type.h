#pragma once
#include "ast_attribute.h"
#include "ast_node.h"
#include "base/string_utils.h"
#include "program_alloc.h"

namespace wgsl::ast {

// Type info node
//   Builtin types:
//     Primitime types: i32, u32, f32
//     Type generators: vec2<T>, vec3<T>, array<T, N>, atomic<T>
//   User types:
//     Structs: struct Foo {};  struct Bar {};
//     Aliases: alias Foo = Bar;
class Type : public Symbol {
public:
    constexpr static inline auto kStaticType = NodeType::Type;

    const std::string_view name;

protected:
    Type(SourceLoc loc, NodeType type, std::string_view name)
        : Symbol(loc, type | kStaticType), name(name) {}
};


// Builtin types
#define SCALAR_KIND_LIST(V) \
    V(Bool, "bool")         \
    V(Int, "int")           \
    V(Float, "float")       \
    V(U32, "u32")           \
    V(I32, "i32")           \
    V(F32, "f32")           \
    V(F16, "f16")

enum class ScalarKind {
#define ENUM(NAME, STR) NAME,
    SCALAR_KIND_LIST(ENUM)
#undef ENUM
};

constexpr std::string_view to_string(ScalarKind type) {
#define CASE(NAME, STR) \
    case ScalarKind::NAME: return STR;
    switch (type) {
        SCALAR_KIND_LIST(CASE)
        default: return "";
    }
#undef CASE
}


// Builtin primitive type
// i32, u32, f32, abstr_int, abst_float
class Scalar final : public Type {
public:
    ScalarKind kind;

public:
    constexpr static auto kStaticType = NodeType::Scalar;

    Scalar(ScalarKind kind)
        : Type({}, kStaticType, to_string(kind)), kind(kind) {}

public:
    constexpr bool IsArithmetic() const { return IsInteger() || IsFloat(); }

    constexpr bool IsAbstract() const {
        return kind == ScalarKind ::Float || kind == ScalarKind ::Int;
    }

    constexpr bool IsSigned() const { return kind != ScalarKind ::U32; }

    constexpr bool IsBool() const { return kind == ScalarKind::Bool; }

    constexpr bool IsScalar() const {
        return IsInteger() || IsFloat() || IsBool();
    }

    constexpr bool IsInteger() const {
        switch (kind) {
            case ScalarKind::U32:
            case ScalarKind::I32:
            case ScalarKind::Int: return true;
            default: return false;
        }
    }

    constexpr bool IsFloat() const {
        switch (kind) {
            case ScalarKind::F32:
            case ScalarKind::Float: return true;
            default: return false;
        }
    }

    constexpr uint32_t GetConversionRankTo(const ast::Scalar* other) const {
        constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
        if (this == other) {
            return 0;
        }
        // Automatic conversion table for abstract kinds
        // https://www.w3.org/TR/WGSL/#conversion-rank
        if (kind == ScalarKind::Float) {
            switch (other->kind) {
                case ScalarKind::F32: return 1;
                case ScalarKind::F16: return 2;
                default: return kMax;
            }
        }
        if (kind == ScalarKind::Int) {
            switch (other->kind) {
                case ScalarKind::I32: return 3;
                case ScalarKind::U32: return 4;
                case ScalarKind::Float: return 5;
                case ScalarKind::F32: return 6;
                case ScalarKind::F16: return 7;
                default: return kMax;
            }
        }
        // Concrete kinds are not convertible
        return kMax;
    }

    constexpr bool AutoConvertibleTo(const ast::Scalar* other) const {
        if (this == other) {
            return true;
        }
        if (!IsAbstract()) {
            return kind == other->kind;
        }
        if (kind == ScalarKind::Float) {
            return other->kind == ScalarKind::F32 ||
                   other->kind == ScalarKind::F16;
        }
        // Abstract int is convertible to all other kinds
        return true;
    }
};


// Vector subtypes
#define VECTOR_KIND_LIST(V) \
    V(Vec2, "vec2")         \
    V(Vec3, "vec3")         \
    V(Vec4, "vec4")

enum class VecKind {
#define ENUM(NAME, STR) NAME,
    VECTOR_KIND_LIST(ENUM)
#undef ENUM
};

constexpr std::string_view to_string(VecKind kind) {
#define CASE(NAME, STR) \
    case VecKind::NAME: return STR;
    switch (kind) {
        VECTOR_KIND_LIST(CASE)
        default: return "";
    }
#undef CASE
}

constexpr std::optional<VecKind> VecKindFromString(std::string_view str) {
#define IF_ELSE(NAME, STR)    \
    if (str == STR)           \
        return VecKind::NAME; \
    else
    VECTOR_KIND_LIST(IF_ELSE)
    return std::nullopt;
#undef IF_ELSE
}

// Builtin vector: vec2, vec3
class Vec final : public Type {
public:
    const VecKind kind;
    const Scalar* valueType;

public:
    constexpr static auto kStaticType = NodeType::Vector;

    Vec(VecKind kind, const Scalar* valueType)
        : Type({}, kStaticType, to_string(kind))
        , kind(kind)
        , valueType(valueType) {}
};


// Builtin array: array<f32, 10>
class Array final : public Type {
public:
    const Type* valueType;
    const uint32_t size;

public:
    constexpr static auto kStaticType = NodeType::Array;

    Array(const Type* valueType, uint32_t size, std::string_view fullName)
        : Type({}, kStaticType, fullName), valueType(valueType), size(size) {}
};



// Matrix subtypes
#define MATRIX_KIND_LIST(V) \
    V(Mat2x2, "mat2x2")     \
    V(Mat2x3, "mat2x3")     \
    V(Mat2x4, "mat2x4")     \
    V(Mat3x2, "mat3x2")     \
    V(Mat3x3, "mat3x3")     \
    V(Mat3x4, "mat3x4")     \
    V(Mat4x2, "mat4x2")     \
    V(Mat4x3, "mat4x3")     \
    V(Mat4x4, "mat4x4")

enum class MatrixKind {
#define ENUM(NAME, STR) NAME,
    MATRIX_KIND_LIST(ENUM)
#undef ENUM
};

constexpr std::string_view to_string(MatrixKind kind) {
#define CASE(NAME, STR) \
    case MatrixKind::NAME: return STR;
    switch (kind) {
        MATRIX_KIND_LIST(CASE)
        default: return "";
    }
#undef CASE
}

constexpr std::optional<MatrixKind> MatrixKindFromString(std::string_view str) {
#define IF_ELSE(NAME, STR)       \
    if (str == STR)              \
        return MatrixKind::NAME; \
    else
    MATRIX_KIND_LIST(IF_ELSE)
    return std::nullopt;
#undef IF_ELSE
}



// Builtin matrix: mat2x3<f32>
class Matrix final : public Type {
public:
    const MatrixKind kind;
    const Type* valueType;

public:
    constexpr static auto kStaticType = NodeType::Matrix;

    Matrix(MatrixKind kind, const Type* valueType)
        : Type({}, kStaticType, to_string(kind))
        , kind(kind)
        , valueType(valueType) {}
};



// Texture subtypes
#define TEXTURE_KIND_LIST(V)                      \
    V(Tex1d, "texture_1d")                        \
    V(Tex2d, "texture_2d")                        \
    V(Tex2dArray, "texture_2d_array")             \
    V(Tex3d, "texture_3d")                        \
    V(TexCube, "texture_cube")                    \
    V(TexCubeArray, "texture_cube_array")         \
    V(TexMS2D, "texture_multisampled_2d")         \
    V(TexStor1D, "texture_storage_1d")            \
    V(TexStor2D, "texture_storage_2d")            \
    V(TexStor2DArray, "texture_storage_2d_array") \
    V(TexStor3D, "texture_storage_3d")

enum class TextureKind {
#define ENUM(NAME, STR) NAME,
    TEXTURE_KIND_LIST(ENUM)
#undef ENUM
};

constexpr std::string_view to_string(TextureKind kind) {
#define CASE(NAME, STR) \
    case TextureKind::NAME: return STR;
    switch (kind) {
        TEXTURE_KIND_LIST(CASE)
        default: return "";
    }
#undef CASE
}

constexpr std::optional<TextureKind> TextureKindFromString(
    std::string_view str) {
#define IF_ELSE(NAME, STR)        \
    if (str == STR)               \
        return TextureKind::NAME; \
    else
    TEXTURE_KIND_LIST(IF_ELSE)
    return std::nullopt;
#undef IF_ELSE
}


// Builtin texture
class Texture final : public Type {
public:
    const TextureKind kind;

public:
    constexpr static auto kStaticType = NodeType::Texture;

    Texture(TextureKind kind)
        : Type({}, kStaticType, to_string(kind)), kind(kind) {}
};



// Builtin sampler: sampler
class Sampler final : public Type {
public:
    constexpr static auto kStaticType = NodeType::Sampler;

    Sampler() : Type({}, kStaticType, "sampler") {}
};



class Member;
using MemberList = ProgramList<const Member*>;

// User defined struct
class Struct final : public Type {
public:
    static constexpr auto kStaticType = NodeType::Struct;

    const SourceLoc loc;
    const MemberList members;

public:
    Struct(SourceLoc loc, std::string_view name, MemberList&& members)
        : Type(loc, kStaticType, name), members(std::move(members)) {}
};

// A struct field
class Member final : public Node {
public:
    constexpr static inline auto kStaticType = NodeType::Member;

    const std::string_view name;
    const Type* type;
    const AttributeList attributes;

public:
    Member(SourceLoc loc,
           std::string_view name,
           const Type* type,
           AttributeList&& attributes)
        : Node(loc, kStaticType)
        , name(name)
        , type(type)
        , attributes(std::move(attributes)) {}
};

}  // namespace wgsl::ast