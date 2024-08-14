#pragma once
#include <variant>
#include "runtime/gpu/common.h"
#include "runtime/string_utils.h"


namespace gpu {

class ShaderDSLContext;



// A virtual address of a Node
// Used for node referencing and as IDs
class Address {
public:
    Address() = default;
    auto operator<=>(const Address& rhs) const = default;

    uint32_t Value() const { return address_; }
    explicit operator bool() const { return address_ != kInvalidIndex; }
    explicit operator uint32_t() const { return address_; }

protected:
    // Could be created in ShaderDSLContext or by subclasses
    Address(uint32_t addr) : address_(addr) {}
    friend class ShaderDSLContext;

private:
    constexpr static uint32_t kInvalidIndex = 0;
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
    DataType dataType;
    std::string identifier;

protected:
    AstNode(Type type) : nodeType(type | Type::AstNode) {}
};

template <std::convertible_to<AstNode> T>
constexpr T* CastNode(AstNode* node) {
    if (node->nodeType & T::GetStaticType()) {
        return static_cast<T*>(node);
    }
    return nullptr;
}

template <std::convertible_to<AstNode> T>
constexpr const T* CastNode(const AstNode* node) {
    if (node->nodeType & T::GetStaticType()) {
        return static_cast<const T*>(node);
    }
    return nullptr;
}


// Top level node type common for functions and structs
class Scope : public AstNode {
public:
    Scope(Address parent, Type type = AstNode::Type::Scope)
        : AstNode(type | AstNode::Type::Scope), parent(parent) {}

    static AstNode::Type GetStaticType() { return AstNode::Type::Scope; }

    Address parent;
    std::vector<Address> children;
};


// A class/struct declaration with variable declarations inside
class Class final : public Scope {
public:
    Class(Address parent) : Scope(parent, Type::Class) {}
    static AstNode::Type GetStaticType() { return Type::Class; }

    Attribute attr;
};

class Function final : public Scope {
public:
    Function(Address parent, std::string_view identifier = "")
        : Scope(parent, Type::Function) {
        this->identifier = identifier;
    }

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
        , attr(attr)
        , semantic(semantic)
        , semanticIdx(0)
        , bindIdx(bindIdx) {
        this->dataType = type;
    }

    static AstNode::Type GetStaticType() { return AstNode::Type::Variable; }

    Attribute attr;
    Semantic semantic;
    uint32_t semanticIdx;
    BindIndex bindIdx;
};

// Constant value passed directly
class Literal final : public AstNode {
private:
    Literal() : AstNode(AstNode::Type::Literal) {}

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

    uint32_t GetUint() {
        DASSERT(dataType == DataType::Uint);
        auto val64 = std::get<int64_t>(value);
        return static_cast<uint32_t>(val64);
    }

    int32_t GetInt() {
        DASSERT(dataType == DataType::Int);
        auto val64 = std::get<int64_t>(value);
        return static_cast<int32_t>(val64);
    }

    float GetFloat() {
        DASSERT(dataType == DataType::Float);
        return std::get<float>(value);
    }

    std::variant<int64_t, float> value;
};

// Code of the operation
enum class OpCode {
    Unknown,
    Mul,
    Div,
    Add,
    Sub,
    Assignment,   // var0 = var1;
    Compare,      // var0 == var1
    FieldAccess,  // var0 == var1.x
    Call,
    // Intrinsics
    SampleTexture,
    ConstructFloat,
    ConstructFloat2,
    ConstructFloat3,
    ConstructFloat4,
};

// An operation or declaration inside a function
class Expression : public AstNode {
public:
    template <std::convertible_to<Address>... Args>
    Expression(OpCode opcode, Args&&... args)
        : AstNode(AstNode::Type::Expression), opcode(opcode) {
        (arguments.push_back(args), ...);
    }
    static AstNode::Type GetStaticType() { return AstNode::Type::Expression; }

    OpCode opcode = OpCode::Unknown;
    std::vector<Address> arguments;
};

// NOTE: Only generic codes
// Code generator should parse internal operations differently
constexpr std::string to_string(OpCode opcode) {
    switch (opcode) {
        case OpCode::Add: return "+";
        case OpCode::Sub: return "-";
        case OpCode::Mul: return "*";
        case OpCode::Div: return "/";
        case OpCode::Assignment: return "=";
        case OpCode::Compare: return "==";
        case OpCode::FieldAccess: return ".";
        default: return "";
    }
}

}  // namespace internal
}  // namespace gpu

DEFINE_TOSTRING_FORMATTER(gpu::Semantic);
DEFINE_TOSTRING_FORMATTER(gpu::internal::OpCode);

DEFINE_CAST_FORMATTER(gpu::Address, uint32_t);