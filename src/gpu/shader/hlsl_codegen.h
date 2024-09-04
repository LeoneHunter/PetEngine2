#pragma once
#include "gpu/common.h"
#include "gpu/gpu.h"
#include "gpu/d3d12/common.h"
#include "shader_dsl.h"

namespace gpu::internal {

// Generates hlsl code from a AstNode tree
class HLSLCodeGenerator : public ShaderCodeGenerator {
public:
    constexpr static auto kMainFuncIdentifier = "main";
    constexpr static auto kLocalPrefix = "local";
    constexpr static auto kGlobalPrefix = "global";
    constexpr static auto kClassPrefix = "Struct";
    constexpr static auto kFuncPrefix = "func";
    constexpr static auto kFieldPrefix = "field";
    constexpr static auto kIndentSize = 4;

    HLSLCodeGenerator() = default;
    using Context = ShaderDSLContext;

    std::unique_ptr<gpu::ShaderCode> Generate(ShaderUsage type,
                                              std::string_view main,
                                              Context* ctx) override;

private:
    void ParseGlobal(Scope* root);
    void ParseFunction(Function* func);

private:
    DataType GetComponentType(const AstNode* node);
    std::string ComponentFromLiteral(const AstNode* node);

    void WriteExpression(Expression* expr, Address addr);

    std::string WriteStruct(const std::string& type,
                            const std::vector<Variable*>& fields);

    std::string GetHLSLRegisterPrefix(const Variable* var);

    void WriteVarDeclaration(Variable* var,
                             bool close = true,
                             std::string_view prefix = "");
    void WriteLiteral(Literal* lit);

private:
    template <class... Args>
    void Write(const std::format_string<Args...> fmt, Args&&... args);

    void WriteIndent();

    template <class... Args>
    void WriteLine(const std::format_string<Args...> fmt, Args&&... args);

    template <std::convertible_to<AstNode> T = AstNode>
    T* NodeFromAddress(Address addr);

    // Scope unique identifier. E.g. local_0, global_3, local_3
    std::string CreateVarIdentifier(std::string_view customPrefix = "");
    std::string CreateClassIdentifier(std::string_view customPrefix = "");
    std::string CreateFuncIdentifier(std::string_view customPrefix = "");
    uint32_t CreateRegisterIndex(d3d12::RegisterType type);

    void ValidateIdentifier(AstNode* node, std::string_view prefix = "");

private:
    void SetIndent(uint32_t indent) { indent_ = indent; }
    void PushIndent() { ++indent_; }
    void PopIndent() { indent_ != 0 ? --indent_ : indent_; }

private:
    // Pointer to the owner of the nodes
    Context* context_ = nullptr;
    ShaderCode* code_ = nullptr;
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
    // Register counters for cbv, srv, uav
    uint32_t regCounter[3]{};
};

}  // namespace gpu::internal

namespace gpu {
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
}  // namespace gpu

DEFINE_TOSTRING_FORMATTER(gpu::DataType);



/*============================================================================*/
namespace gpu {
namespace internal {

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
    DASSERT(context_);
    AstNode* node = context_->NodeFromAddress(addr);
    if (!node || !(node->nodeType & T::GetStaticType())) {
        return nullptr;
    }
    return static_cast<T*>(node);
}

// Scope unique identifier. E.g. local_0, global_3, local_3
inline std::string HLSLCodeGenerator::CreateVarIdentifier(
    std::string_view customPrefix) {

    auto prefix = customPrefix;
    if (prefix.empty()) {
        prefix = isInsideFunction_ ? kLocalPrefix : kGlobalPrefix;
    }
    const auto id =
        isInsideFunction_ ? localVarCounter_++ : globalVarCounter_++;
    return std::format("{}_{}", prefix, id);
}

inline std::string HLSLCodeGenerator::CreateClassIdentifier(
    std::string_view customPrefix) {
    return std::format("{}_{}",
                       customPrefix.empty() ? kClassPrefix : customPrefix,
                       structCounter_++);
}

inline std::string HLSLCodeGenerator::CreateFuncIdentifier(
    std::string_view customPrefix) {
    return std::format("{}_{}",
                       customPrefix.empty() ? kFuncPrefix : customPrefix,
                       funcCounter_++);
}

inline uint32_t HLSLCodeGenerator::CreateRegisterIndex(
    d3d12::RegisterType type) {
    switch(type) {
        case d3d12::RegisterType::CBV: {
            return regCounter[(uint32_t)d3d12::RegisterType::CBV]++;
        }
        case d3d12::RegisterType::SRV: {
            return regCounter[(uint32_t)d3d12::RegisterType::SRV]++;
        }
        case d3d12::RegisterType::UAV: {
            return regCounter[(uint32_t)d3d12::RegisterType::UAV]++;
        }
    }
    return 0;
}

inline void HLSLCodeGenerator::ValidateIdentifier(
    AstNode* node,
    std::string_view customPrefix) {

    if (node->identifier.empty()) {
        if (Variable* var = CastNode<Variable>(node)) {
            var->identifier = CreateVarIdentifier(customPrefix);
        } else if (Function* func = CastNode<Function>(node)) {
            func->identifier = CreateFuncIdentifier(customPrefix);
        } else if (Class* cl = CastNode<Class>(node)) {
            cl->identifier = CreateClassIdentifier(customPrefix);
        } else if (Expression* expr = CastNode<Expression>(node)) {
            expr->identifier = CreateVarIdentifier(customPrefix);
        } else if (Literal* lit = CastNode<Literal>(node)) {
            lit->identifier = CreateVarIdentifier(customPrefix);
        }
    }
}

}  // namespace internal

using internal::HLSLCodeGenerator;

}  // namespace gpu
