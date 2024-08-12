#pragma once
#include "shader_dsl_ast.h"

namespace gpu {

// Interface to generators
class CodeGenerator {
public:
    virtual ~CodeGenerator() = default;

    // Delegate to a ast tree
    class Delegate {
    public:
        virtual gpu::internal::AstNode* NodeFromAddress(Address addr) = 0;
    };

    virtual std::unique_ptr<ShaderCode> Generate(gpu::internal::Scope* root,
                                                 Delegate* ast) = 0;
};

// Sequentually generates a shader code
class ShaderDSLContext final : private CodeGenerator::Delegate {
public:
    static ShaderDSLContext& Current() {
        DASSERT(currentShaderBuilder);
        return *currentShaderBuilder;
    }

    void Clear() {
        nodes_.clear();
        nodes_.reserve(32);
        // Reserve 0 address as invalid
        nodes_.emplace_back();
        const Address addr = NewNode<internal::Scope>(kInvalidAddress);
        currentScopeAddr_ = addr;
    }

    std::unique_ptr<ShaderCode> BuildHLSL(CodeGenerator* generator,
                                          std::string_view entryFuncName = "") {
        internal::Scope* root =
            NodeFromAddress<internal::Scope>(kRootNodeAddress);
        DASSERT(root);
        DASSERT(generator);
        std::unique_ptr<ShaderCode> code = generator->Generate(root, this);
        return code;
    }

    ShaderDSLContext() {
        DASSERT(!currentShaderBuilder);
        currentShaderBuilder = this;
        Clear();
    }

    ~ShaderDSLContext() { currentShaderBuilder = nullptr; }

public:
    // Declares a global, local or class variable
    Address DeclareVar(DataType type,
                       Attribute attr = Attribute::None,
                       Semantic semantic = Semantic::None,
                       BindIndex bindIdx = kInvalidBindIndex) {
        const Address var =
            NewNode<internal::Variable>(type, attr, semantic, bindIdx);
        internal::Scope* scope =
            NodeFromAddress<internal::Scope>(currentScopeAddr_);
        scope->children.push_back(var);
        return var;
    }

    void SetAttribute(Address addr, Attribute attr) {
        DASSERT(addr);
        internal::Variable* var = NodeFromAddress<internal::Variable>(addr);
        DASSERT(var);
        var->attr = attr;
    }

    void SetBindIndex(Address addr, BindIndex bindIdx) {
        DASSERT(addr);
        internal::Variable* var = NodeFromAddress<internal::Variable>(addr);
        DASSERT(var);
        DASSERT(var->semantic == Semantic::None);
        var->bindIdx = bindIdx;
    }

    void SetIdentifier(Address addr, const std::string& identifier) {
        DASSERT(addr);
        if (identifier.empty()) {
            return;
        }
        internal::AstNode* node = NodeFromAddress(addr);
        DASSERT(node);
        node->identifier = identifier;
        return;
    }

    void SetSemantic(Address addr, Semantic semantic) {
        DASSERT(addr);
        internal::Variable* var = NodeFromAddress<internal::Variable>(addr);
        DASSERT(var);
        DASSERT(var->bindIdx == kInvalidBindIndex);
        var->semantic = semantic;
    }

    // float2 vec; vec.x
    Address OpFieldAccess(Address addr, uint32_t index) {
        DASSERT(IsInScope<internal::Function>());
        internal::Function* func = GetCurrentScope<internal::Function>();
        const Address literalNode =
            NewNode<internal::Literal>(DataType::Uint, index);
        const Address expr = NewNode<internal::Expression>(
            internal::Expression::OpCode::FieldAccess, addr, literalNode);
        func->children.push_back(expr);
        return expr;
    }

    // Constant inline value. E.g. 1.f, 345
    Address DeclareLiteral(DataType type, float val) {
        internal::Scope* scope = GetCurrentScope();
        DASSERT(scope);
        Address node = NewNode<internal::Literal>(type, val);
        scope->children.push_back(node);
        return node;
    }

    Address DeclareFunction() {
        DASSERT(!IsInScope<internal::Function>());
        Address addr = NewNode<internal::Function>(currentScopeAddr_);
        PushScope(addr);
        return addr;
    }

    void DeclareFunctionEnd() {
        DASSERT(IsInScope<internal::Function>());
        PopScope();
    }

    template <std::convertible_to<Address>... Args>
    Address PushExpression(internal::Expression::OpCode opcode, Args... args) {
        internal::Function* scope = GetCurrentScope<internal::Function>();
        DASSERT(scope);
        Address node = NewNode<internal::Expression>(opcode, args...);
        scope->children.push_back(node);
        return node;
    }

private:
    internal::AstNode* NodeFromAddress(Address addr) override {
        DASSERT(addr);
        DASSERT(addr.Value() < nodes_.size());
        return nodes_[addr.Value()].get();
    }

    // Creates a new node in the node list
    template <std::derived_from<internal::AstNode> T, class... Args>
    Address NewNode(Args&&... args) {
        auto* node = new T(std::forward<Args>(args)...);
        const Address addr = GetNextAddr();
        nodes_.emplace_back(node);
        return addr;
    }

    void PushScope(Address nodeAddr) {
        if (currentScopeAddr_) {
            internal::Scope* scope =
                NodeFromAddress<internal::Scope>(currentScopeAddr_);
            scope->children.push_back(nodeAddr);
        }
        currentScopeAddr_ = nodeAddr;
    }

    void PopScope() {
        DASSERT(currentScopeAddr_);
        internal::Scope* scope =
            NodeFromAddress<internal::Scope>(currentScopeAddr_);
        DASSERT_M(scope->parent, "Cannot pop scope, already root");
        currentScopeAddr_ = scope->parent;
    }

    template <std::convertible_to<internal::Scope> T = internal::Scope>
    T* GetCurrentScope() {
        if (!currentScopeAddr_) {
            return nullptr;
        }
        return NodeFromAddress<T>(currentScopeAddr_);
    }

    template <std::convertible_to<internal::Scope> T>
    bool IsInScope() {
        return GetCurrentScope<T>() != nullptr;
    }

    template <std::derived_from<internal::AstNode> T>
    T* NodeFromAddress(Address addr) {
        DASSERT(addr);
        DASSERT(addr.Value() < nodes_.size());
        internal::AstNode* node = nodes_[addr.Value()].get();
        // Check type bits in the node
        return CastNode<T>(node);
    }

    Address GetNextAddr() const { return {(uint32_t)nodes_.size()}; }

    template <class... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args) {
        DASSERT_F(false, fmt, std::forward<Args>(args)...);
    }

private:
    constexpr static int kRootNodeAddress = 1;

    // Node storage index by Address
    // 0 node is a global scope node
    // TODO: Use arena alloc here
    std::vector<std::unique_ptr<internal::AstNode>> nodes_;
    Address currentScopeAddr_;

private:
    // For context-less functions. E.g. Output o = Input(4) * Input(5);
    static inline thread_local ShaderDSLContext* currentShaderBuilder = nullptr;
};


// Shader builder DSL language api
// Each function call and object creation uses thread_local ShaderBuilder to
//   record that operation into a syntax tree
// NOTE: internal::Variable names used only in debug mode
namespace shader_dsl {

namespace internal {

// Registers type assignments
struct TypeBase : public Address {
    TypeBase& operator=(const TypeBase& rhs) {
        Address& self = static_cast<Address&>(*this);
        ShaderDSLContext::Current().PushExpression(
            gpu::internal::Expression::OpCode::Assign, self, rhs);
        self = rhs;
        return *this;
    }
};

}  // namespace internal

// Custom user definen identifier for functions and variables
class Identifier {
public:
    Identifier(std::string_view name) : str(name) {}
    std::string str;
};

class Float : public internal::TypeBase {
public:
    static DataType GetType() { return DataType::Float; }
};

class Float2 : public internal::TypeBase {
public:
    static DataType GetType() { return DataType::Float2; }
};

class Float3 : public internal::TypeBase {
public:
    static DataType GetType() { return DataType::Float3; }
};

class Float4 : public internal::TypeBase {
public:
    static DataType GetType() { return DataType::Float4; }
};

class Float4x4 : public internal::TypeBase {
public:
    static DataType GetType() { return DataType::Float4x4; }
};

class Sampler : public internal::TypeBase {
public:
    static DataType GetType() { return DataType::Sampler; }
};

class Texture2D : public internal::TypeBase {
public:
    static DataType GetType() { return DataType::Texture2D; }
};

// Sets a custom identifier for a function, struct or variable
template <std::derived_from<Address> T>
T operator|(T var, const Identifier& name) {
    ShaderDSLContext::Current().SetIdentifier(var, name.str);
    return var;
}

template <std::derived_from<Address> T>
T operator|(T var, BindIndex bindIdx) {
    ShaderDSLContext::Current().SetBindIndex(var, bindIdx);
    return var;
}

template <std::derived_from<Address> T>
T operator|(T var, Semantic semantic) {
    ShaderDSLContext::Current().SetSemantic(var, semantic);
    return var;
}

template <std::derived_from<Address> T>
T operator|(T var, Attribute attr) {
    ShaderDSLContext::Current().SetAttribute(var, attr);
    return var;
}

template <std::convertible_to<Address> T>
T Declare(std::string_view identifier = "") {
    T addr = {ShaderDSLContext::Current().DeclareVar(T::GetType())};
    addr | identifier;
    return addr;
}

// A shader parameter if defined in global scope
// A function parameter if defined in a function
template <std::convertible_to<Address> T>
T Input(std::string_view identifier = "") {
    return Declare<T>(identifier) | Attribute::Input;
}

// A shader return parameter if defined in global scope
// A function return parameter if defined in a function
template <std::convertible_to<Address> T>
T Output(std::string_view identifier = "") {
    return Declare<T>(identifier) | Attribute::Output;
}

// Define an input constant
template <std::convertible_to<Address> T>
T Uniform(std::string_view identifier = "") {
    return Declare<T>(identifier) | Attribute::Uniform;
}

template <std::convertible_to<Address> T>
T operator*(T lhs, T rhs) {
    return {ShaderDSLContext::Current().PushExpression(
        gpu::internal::Expression::OpCode::Mul, lhs, rhs)};
}

template <std::convertible_to<Address> T>
T operator*(T lhs, float scalar) {
    Address rhs =
        ShaderDSLContext::Current().DeclareLiteral(DataType::Float, scalar);
    return {ShaderDSLContext::Current().PushExpression(
        gpu::internal::Expression::OpCode::Mul, lhs, rhs)};
}

inline Float4 operator*(Float4x4 mat, Float4 vec) {
    return {ShaderDSLContext::Current().PushExpression(
        gpu::internal::Expression::OpCode::Mul, mat, vec)};
}

inline Float GetX(Float2 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 0)};
}
inline Float GetX(Float3 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 0)};
}
inline Float GetX(Float4 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 0)};
}

inline Float GetY(Float2 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 1)};
}
inline Float GetY(Float3 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 1)};
}
inline Float GetY(Float4 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 1)};
}

inline Float GetZ(Float3 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 2)};
}
inline Float GetZ(Float4 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 2)};
}

inline Float GetW(Float4 vec) {
    return {ShaderDSLContext::Current().OpFieldAccess(vec, 3)};
}

inline Float CreateFloat(float val) {
    return {ShaderDSLContext::Current().DeclareLiteral(DataType::Float, val)};
}

inline Float4 CreateFloat2(Address x, Address y) {
    return {ShaderDSLContext::Current().PushExpression(
        gpu::internal::Expression::OpCode::ConstructFloat2, x, y)};
}

inline Float4 CreateFloat3(Address x, Address y, Address z) {
    return {ShaderDSLContext::Current().PushExpression(
        gpu::internal::Expression::OpCode::ConstructFloat3, x, y, z)};
}

inline Float4 CreateFloat4(Address x, Address y, Address z, Address w) {
    return {ShaderDSLContext::Current().PushExpression(
        gpu::internal::Expression::OpCode::ConstructFloat4, x, y, z, w)};
}

inline Float4 SampleTexture(Texture2D texture, Sampler sampler, Float2 uv) {
    return {ShaderDSLContext::Current().PushExpression(
        gpu::internal::Expression::OpCode::SampleTexture, texture, sampler,
        uv)};
}

inline Address Function(std::string_view identifier = "") {
    Address func = {ShaderDSLContext::Current().DeclareFunction()};
    func | identifier;
    return func;
}

inline void EndFunction() {
    ShaderDSLContext::Current().DeclareFunctionEnd();
}


}  // namespace shader_dsl
}  // namespace gpu
