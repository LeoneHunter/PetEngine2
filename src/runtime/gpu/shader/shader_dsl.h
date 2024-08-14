#pragma once
#include "shader_dsl_ast.h"

namespace gpu {

// Sequentually generates a shader code
class ShaderDSLContext {
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

    ShaderDSLContext() {
        DASSERT(!currentShaderBuilder);
        currentShaderBuilder = this;
        Clear();
    }

    ~ShaderDSLContext() { currentShaderBuilder = nullptr; }

public:
    // Declares a global, local or class variable
    Address DeclareVar(DataType type) {
        const Address var = NewNode<internal::Variable>(type);
        internal::Scope* scope =
            NodeFromAddress<internal::Scope>(currentScopeAddr_);
        scope->children.push_back(var);
        return var;
    }

    // Defines (aka calls constructor) a global, local or class variable
    // Combines a declaration and assignment
    template <std::convertible_to<Address>... Args>
    Address DefineVar(DataType type, Args&&... args) {
        const Address var = NewNode<internal::Variable>(type);
        internal::Scope* scope =
            NodeFromAddress<internal::Scope>(currentScopeAddr_);
        scope->children.push_back(var);

        using OpCode = internal::OpCode;
        OpCode opcode = OpCode::Unknown;
        switch (type) {
            case DataType::Float2: {
                opcode = OpCode::ConstructFloat2;
                break;
            }
            case DataType::Float3: {
                opcode = OpCode::ConstructFloat3;
                break;
            }
            case DataType::Float4: {
                opcode = OpCode::ConstructFloat4;
                break;
            }
            default: {
                DASSERT_F(false, "Unknown type of definition. {}", type);
            }
        }
        const Address expr =
            Expression(opcode, var, std::forward<Args>(args)...);
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

    void SetIdentifier(Address addr, std::string_view identifier) {
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
            internal::OpCode::FieldAccess, addr, literalNode);
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
    Address Expression(internal::OpCode opcode, Args... args) {
        internal::Function* scope = GetCurrentScope<internal::Function>();
        DASSERT(scope);
        Address node = NewNode<internal::Expression>(opcode, args...);
        scope->children.push_back(node);
        return node;
    }

public:
    internal::AstNode* NodeFromAddress(Address addr) {
        DASSERT(addr);
        DASSERT(addr.Value() < nodes_.size());
        return nodes_[addr.Value()].get();
    }

    internal::Scope* GetRoot() {
        DASSERT(nodes_.size() > kRootNodeAddress);
        return NodeFromAddress<internal::Scope>(kRootNodeAddress);
    }

private:
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

// Abstract base class for DSL types
// Contains a reference to Expression, Variable or Literal nodes
// Registers type assignments
class TypeBase {
public:
    TypeBase& operator=(const TypeBase& rhs) {
        ShaderDSLContext::Current().Expression(
            gpu::internal::OpCode::Assignment, addr_, rhs.addr_);
        addr_ = rhs.addr_;
        return *this;
    }

    Address GetAddress() const { return addr_; }

protected:
    TypeBase() = default;

    TypeBase(Address addr) : addr_(addr) {}

    void SetAddress(Address addr) { addr_ = addr; }

private:
    Address addr_;
};

}  // namespace internal
}  // namespace shader_dsl
}  // namespace gpu
