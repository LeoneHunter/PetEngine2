#pragma once
#include "runtime/gpu/common.h"
#include "runtime/string_utils.h"
#include <variant>


namespace gpu {

class ShaderDSLContext;

// Predefined bind index for shader inputs and outputs
// NOTE: Semantic and bind index are mutually exclusive
enum class Semantic {
    None,
    Position,
    Color,
    Texcoord,
};

constexpr std::string to_string(Semantic semantic) {
    switch (semantic) {
        case Semantic::Position: return "Position";
        case Semantic::Color: return "Color";
        case Semantic::Texcoord: return "Texcoord";
    }
    return "";
}

// Additional property that can be added to variables
enum class Attribute {
    None,
    // Shader or function input parameter
    Input,
    // Shader or function return parameter
    Output,
    // Shader constant input parameter
    Uniform,
};

// A type of a variable, parameter, field. (memory location)
// Basically a builder local id
enum class DataType: uint32_t {
    Unknown,
    Float,
    Float2,
    Float3,
    Float4,
    Float4x4,
    Uint,
    Function,
    Class,
    Sampler,
    Texture2D,
    // TODO: Implement other types
};

constexpr std::string to_string(DataType type) {
    switch (type) {
        case DataType::Float: return "float";
        case DataType::Float2: return "float2";
        case DataType::Float3: return "float3";
        case DataType::Float4: return "float4";
        case DataType::Float4x4: return "float4x4";
        case DataType::Sampler: return "SamplerState";
        case DataType::Texture2D: return "Texture2D";
    }
    return "";
}


// Generated shader data
class ShaderCode {
public:
    std::string code;
};

// A virtual address of a Node
// Used for node referencing and as IDs
class Address {
public:
    Address() = default;
    auto operator<=>(const Address& rhs) const = default;

    uint32_t Value() const { return address_; }
    explicit operator bool() const { return address_ != kInvalidIndex; }
    explicit operator uint32_t() const { return address_; }

private:
    Address(uint32_t addr) : address_(addr) {}
    friend class ShaderDSLContext;

    constexpr static uint32_t kInvalidIndex = 0;

private:
    uint32_t address_ = kInvalidIndex;
};

constexpr Address kInvalidAddress{};


namespace internal {

// Abstract base class for syntax tree nodes
class AstNode {
public:
    // Used for type casting
    enum class Type {
        AstNode = 0x1,
        Scope = 0x2,
        Class = 0x4,
        Function = 0x8,
        Variable = 0x10,
        Literal = 0x20,
        Expression = 0x40,
    };
    DEFINE_ENUM_FLAGS_OPERATORS_FRIEND(Type)

    static AstNode::Type GetStaticType() { return AstNode::Type::AstNode; }

    Type nodeType;
    std::string identifier;

protected:
    AstNode(Type type): nodeType(type | Type::AstNode) {}
};

template<std::convertible_to<AstNode> T>
constexpr T* CastNode(AstNode* node) {
    if(node->nodeType & T::GetStaticType()) {
        return static_cast<T*>(node);
    }
    return nullptr;
}

template<std::convertible_to<AstNode> T>
constexpr const T* CastNode(const AstNode* node) {
    if(node->nodeType & T::GetStaticType()) {
        return static_cast<const T*>(node);
    }
    return nullptr;
}


// Top level node type common for functions and structs
class Scope: public AstNode {
public:
    Scope(Address parent, Type type = AstNode::Type::Scope)
        : AstNode(type | AstNode::Type::Scope), parent(parent) {}

    static AstNode::Type GetStaticType() { return AstNode::Type::Scope; }

    Address parent;
    std::vector<Address> children;
};


// A class/struct declaration with variable declarations inside
class Class final: public Scope {
public:
    Class(Address parent) : Scope(parent, Type::Class) {}
    static AstNode::Type GetStaticType() { return Type::Class; }

    Attribute attr;
};

class Function final: public Scope {
public:
    Function(Address parent)
        : Scope(parent, Type::Function) {}

    static AstNode::Type GetStaticType() { return Type::Function; }

    std::vector<Address> inputs;
    std::vector<Address> outputs;
};

class Variable final : public AstNode {
public:
    Variable(gpu::DataType type,
             Attribute attr = Attribute::None,
             Semantic semantic = Semantic::None,
             BindIndex bindIdx = kInvalidBindIndex)
        : AstNode(AstNode::Type::Variable)
        , dataType(type)
        , attr(attr)
        , semantic(semantic)
        , semanticIdx(0)
        , bindIdx(bindIdx) {}

    static AstNode::Type GetStaticType() { return AstNode::Type::Variable; }

    gpu::DataType dataType;
    Attribute attr;
    Semantic semantic;
    uint32_t semanticIdx;
    BindIndex bindIdx;
};

// Constant value passed directly
class Literal final: public AstNode {
private:
    Literal(): AstNode(AstNode::Type::Literal) {}

public:
    Literal(gpu::DataType type, float val) : Literal() {
        value.emplace<float>(val);
        dataType = type;
    }

    Literal(gpu::DataType type, int32_t val) : Literal() {
        value.emplace<int64_t>(val);
        dataType = type;
    }

    Literal(gpu::DataType type, uint32_t val) : Literal() {
        value.emplace<int64_t>(val);
        dataType = type;
    }

    static AstNode::Type GetStaticType() { return AstNode::Type::Literal; }

    DataType dataType;
    std::variant<int64_t, float> value;
};


// An operation or declaration inside a function
class Expression : public AstNode {
public:
    // Code of the operation
    enum class OpCode {
        Mul,
        Div,
        Add,
        Sub,
        Assign,
        Compare,
        FieldAccess,
        SampleTexture,
        ConstructFloat2,
        ConstructFloat3,
        ConstructFloat4,
    };

    template <std::convertible_to<Address>... Args>
    Expression(OpCode opcode, Args&&... args)
        : AstNode(AstNode::Type::Expression)
        , opcode(opcode) {
        (arguments.push_back(args), ...);
    }
    static AstNode::Type GetStaticType() { return AstNode::Type::Expression; }

    OpCode opcode;
    std::vector<Address> arguments;
};

constexpr std::string to_string(Expression::OpCode opcode) {
    using OpCode = Expression::OpCode;
    switch (opcode) {
        case OpCode::Add: return "+";
        case OpCode::Sub: return "-";
        case OpCode::Mul: return "*";
        case OpCode::Div: return "/";
        case OpCode::Assign: return "=";
        case OpCode::Compare: return "==";
        case OpCode::FieldAccess: return ".";
        case OpCode::SampleTexture: return "Sample";
        case OpCode::ConstructFloat2: return "float2";
        case OpCode::ConstructFloat3: return "float3";
        case OpCode::ConstructFloat4: return "float4";
    }
    return "";
}


} // namespace internal
} // namespace gpu

DEFINE_TOSTRING_OSTREAM(gpu::DataType);
DEFINE_TOSTRING_OSTREAM(gpu::Semantic);
DEFINE_TOSTRING_OSTREAM(gpu::internal::Expression::OpCode);

DEFINE_TOSTRING_FORMATTER(gpu::DataType);
DEFINE_TOSTRING_FORMATTER(gpu::Semantic);
DEFINE_TOSTRING_FORMATTER(gpu::internal::Expression::OpCode);

DEFINE_CAST_FORMATTER(gpu::Address, uint32_t);