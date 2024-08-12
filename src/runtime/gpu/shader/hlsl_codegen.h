#pragma once
#include "shader_dsl.h"

namespace gpu {
namespace internal {

// Generates hlsl code from a AstNode tree
class HLSLCodeGenerator final : public CodeGenerator {
public:
    constexpr static auto kMainFuncIdentifier = "main";
    constexpr static auto kLocalPrefix = "local";
    constexpr static auto kGlobalPrefix = "global";
    constexpr static auto kClassPrefix = "struct";
    constexpr static auto kFuncPrefix = "func";
    constexpr static auto kIndentSize = 4;

    HLSLCodeGenerator() = default;

    std::unique_ptr<ShaderCode> Generate(internal::Scope* root,
                                         CodeGenerator::Delegate* ast) override;

private:
    void ParseGlobal(Scope* root);
    void ParseFunction(Function* func);

private:
    std::string GetArgumentType(const AstNode* node);
    std::string GetComponentType(const AstNode* node);
    std::string GetComponent(const AstNode* node);

    void WriteExpression(Expression* expr, Address addr);

    std::string WriteStruct(const std::string& type,
                            const std::vector<Variable*>& fields);

    std::string GetHLSLRegisterPrefix(const Variable* var);

    void WriteVarDeclaration(Variable* var, bool close = true);

private:
    template <class... Args>
    void Write(const std::format_string<Args...> fmt, Args&&... args);

    void WriteIndent();

    template <class... Args>
    void WriteLine(const std::format_string<Args...> fmt, Args&&... args);

    template <std::convertible_to<AstNode> T = AstNode>
    T* NodeFromAddress(Address addr);

    // Scope unique identifier. E.g. local_0, global_3, local_3
    std::string CreateVarIdentifier();
    std::string CreateClassIdentifier();
    std::string CreateFuncIdentifier();

    void ValidateIdentifier(AstNode* node);

private:
    void SetIndent(uint32_t indent) { indent_ = indent; }
    void PushIndent() { ++indent_; }
    void PopIndent() { indent_ != 0 ? --indent_ : indent_; }

private:
    // Pointer to the owner of the nodes
    Delegate* ast_ = nullptr;
    Function* main_ = nullptr;
    std::stringstream* stream_ = nullptr;
    // Defines the variables prefix used, global or local
    bool isInsideFunction_ = false;
    // Used for identifiers generation
    uint32_t localVarCounter_ = 0;
    uint32_t globalVarCounter_ = 0;
    uint32_t structCounter_ = 0;
    uint32_t funcCounter_ = 0;
    // Indentation for debugging
    uint32_t indent_ = 0;
};


/*============================================================================*/
template <class... Args>
void HLSLCodeGenerator::Write(const std::format_string<Args...> fmt,
                              Args&&... args) {
    *stream_ << std::format(fmt, std::forward<Args>(args)...);
}

inline void HLSLCodeGenerator::WriteIndent() {
    for (uint32_t i = 0; i < indent_ * kIndentSize; ++i) {
        *stream_ << ' ';
    }
}

template <class... Args>
void HLSLCodeGenerator::WriteLine(const std::format_string<Args...> fmt,
                                  Args&&... args) {
    WriteIndent();
    *stream_ << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

template <std::convertible_to<AstNode> T>
T* HLSLCodeGenerator::NodeFromAddress(Address addr) {
    DASSERT(ast_);
    AstNode* node = ast_->NodeFromAddress(addr);
    if (!node || !(node->nodeType & T::GetStaticType())) {
        return nullptr;
    }
    return static_cast<T*>(node);
}

// Scope unique identifier. E.g. local_0, global_3, local_3
inline std::string HLSLCodeGenerator::CreateVarIdentifier() {
    const auto prefix = isInsideFunction_ ? kLocalPrefix : kGlobalPrefix;
    const auto id =
        isInsideFunction_ ? localVarCounter_++ : globalVarCounter_++;
    return std::format("{}_{}", prefix, id);
}

inline std::string HLSLCodeGenerator::CreateClassIdentifier() {
    return std::format("{}_{}", kClassPrefix, structCounter_++);
}

inline std::string HLSLCodeGenerator::CreateFuncIdentifier() {
    return std::format("{}_{}", kFuncPrefix, funcCounter_++);
}

inline void HLSLCodeGenerator::ValidateIdentifier(AstNode* node) {
    if (node->identifier.empty()) {
        if (Variable* var = CastNode<Variable>(node)) {
            var->identifier = CreateVarIdentifier();
        } else if (Function* func = CastNode<Function>(node)) {
            func->identifier = CreateFuncIdentifier();
        } else if (Class* cl = CastNode<Class>(node)) {
            cl->identifier = CreateClassIdentifier();
        } else if (Expression* expr = CastNode<Expression>(node)) {
            expr->identifier = CreateVarIdentifier();
        }
    }
}

}  // namespace internal

using internal::HLSLCodeGenerator;

}  // namespace gpu
