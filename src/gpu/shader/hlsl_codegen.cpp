#include "hlsl_codegen.h"

namespace gpu::internal {

std::unique_ptr<ShaderCode> HLSLCodeGenerator::Generate(ShaderUsage type,
                                                        std::string_view main,
                                                        Context* ctx) {

    context_ = ctx;
    auto result = std::make_unique<ShaderCode>();
    result->type = type;
    result->mainId = main;
    code_ = result.get();
    std::stringstream stream;
    stream_ = &stream;
    ParseGlobal(ctx->GetRoot());
    result->code = stream.str();
    return result;
}

void HLSLCodeGenerator::ParseGlobal(Scope* root) {
    for (Address addr : root->children) {
        if (Variable* var = NodeFromAddress<Variable>(addr)) {
            if (var->attr == Attribute::Uniform) {
                const std::string type =
                    (var->dataType == DataType::Texture2D) ? "t" : "b";
                const auto uniform = ShaderCode::Uniform{
                    .id = var->identifier,
                    .type = var->dataType,
                    .binding = var->bindIdx,
                    .location = {
                        .type = type,
                        .index = CreateRegisterIndex(
                            var->dataType == DataType::Texture2D
                                ? d3d12::RegisterType::SRV
                                : d3d12::RegisterType::CBV),
                    }};
                code_->uniforms.push_back(uniform);
                // Override the bind index
                var->bindIdx = uniform.binding;
            }
            WriteVarDeclaration(var);
            continue;
        } else if (Function* func = NodeFromAddress<Function>(addr)) {
            ParseFunction(func);
            continue;
        } else if (Literal* literal = NodeFromAddress<Literal>(addr)) {
            WriteLiteral(literal);
            continue;
        }
        DASSERT_M(false, "Unknown node type encountered");
    }
}

void HLSLCodeGenerator::ParseFunction(Function* func) {
    isInsideFunction_ = true;
    const bool isMain = func->identifier == code_->mainId;
    std::vector<Variable*> inputs;
    std::vector<Variable*> outputs;
    std::vector<Expression*> body;
    // Gather inputs and outputs
    for (Address addr : func->children) {
        if (Variable* var = NodeFromAddress<Variable>(addr)) {
            switch (var->attr) {
                case Attribute::Input: {
                    inputs.emplace_back(var);
                    if (isMain) {
                        ShaderCode::Varying input{};
                        input.id = var->identifier;
                        input.semantic = var->semantic;
                        input.type = var->dataType;
                        code_->inputs.push_back(input);
                    }
                    break;
                }
                case Attribute::Output: {
                    outputs.emplace_back(var);
                    if (isMain) {
                        ShaderCode::Varying output{};
                        output.id = var->identifier;
                        output.semantic = var->semantic;
                        output.type = var->dataType;
                        code_->outputs.push_back(output);
                    }
                    break;
                }
            }
        } else if (Expression* expr = NodeFromAddress<Expression>(addr)) {
            body.emplace_back(expr);
        }
    }
    std::string returnType;
    // Determine the return type
    switch (outputs.size()) {
        case 0: returnType = "void"; break;
        case 1: returnType = to_string(outputs.back()->dataType); break;
        default: returnType = CreateClassIdentifier(); break;
    }
    // Write outputs struct declaration
    if (outputs.size() > 1) {
        WriteStruct(returnType, outputs);
    }
    // Write signature
    const std::string funcId =
        func->identifier.empty() ? CreateFuncIdentifier() : func->identifier;
    Write("{} {}(", returnType, funcId);
    // Write arguments
    for (size_t i = 0; i < inputs.size(); ++i) {
        Variable* param = inputs[i];
        if (i > 0) {
            Write(", ");
        }
        WriteVarDeclaration(param, false);
    }
    Write(") ");
    // In pixel shader with single output write semantic
    if (isMain && code_->type == ShaderUsage::Pixel && outputs.size() == 1) {
        const std::string semantic = outputs.back()->semantic == Semantic::Color
                                         ? "SV_Target"
                                         : to_string(outputs.back()->semantic);
        Write(": {}", semantic);
    }
    Write("{}\n", '{');
    PushIndent();
    std::string returnId;
    // Write returns declaration
    switch (outputs.size()) {
        case 0: break;
        case 1: {
            AstNode* var = outputs.back();
            ValidateIdentifier(var);
            WriteLine("{} {};", var->dataType, var->identifier);
            returnId = var->identifier;
            break;
        }
        default: {
            returnId = CreateVarIdentifier();
            WriteLine("{} {};", returnType, returnId);
            // Update output identifiers to struct.var
            for (Variable* ret : outputs) {
                ret->identifier =
                    std::format("{}.{}", returnId, ret->identifier);
            }
        }
    }
    // Write body
    for (Address addr : func->children) {
        if (Expression* expr = NodeFromAddress<Expression>(addr)) {
            WriteExpression(expr, addr);
        } else if (Variable* var = NodeFromAddress<Variable>(addr)) {
            if (var->attr == Attribute::Input ||
                var->attr == Attribute::Output) {
                continue;
            }
            WriteVarDeclaration(var);
        } else if (Literal* literal = NodeFromAddress<Literal>(addr)) {
            WriteLiteral(literal);
        } else {
            NOT_IMPLEMENTED();
        }
    }
    // Write return
    if (outputs.size() > 0) {
        WriteIndent();
        Write("return {};\n", returnId);
    }
    PopIndent();
    Write("{}\n", '}');
    isInsideFunction_ = false;
    localVarCounter_ = 0;
}

DataType HLSLCodeGenerator::GetComponentType(const AstNode* node) {
    switch (node->dataType) {
        case DataType::Float2:
        case DataType::Float3:
        case DataType::Float4: return DataType::Float;
        default: return DataType::Unknown;
    }
}

std::string HLSLCodeGenerator::ComponentFromLiteral(const AstNode* node) {
    if (const Literal* literal = CastNode<Literal>(node)) {
        if (literal->dataType == DataType::Uint) {
            const int64_t index64 = std::get<int64_t>(literal->value);
            if (index64 <= std::numeric_limits<uint32_t>::max()) {
                const uint32_t index32 = (uint32_t)index64;
                switch (index32) {
                    case 0: return "x";
                    case 1: return "y";
                    case 2: return "z";
                    case 3: return "w";
                }
            }
        }
    }
    return "";
}

void HLSLCodeGenerator::WriteExpression(Expression* expr, Address addr) {
    const auto opcode = expr->opcode;
    switch (opcode) {
        case OpCode::Add:
        case OpCode::Sub:
        case OpCode::Mul:
        case OpCode::Div: {
            DASSERT(expr->arguments.size() == 2);
            const AstNode* lhs = NodeFromAddress(expr->arguments[0]);
            const AstNode* rhs = NodeFromAddress(expr->arguments[1]);
            ValidateIdentifier(expr);
            if (opcode == OpCode::Mul && lhs->dataType == DataType::Float4x4) {
                expr->dataType = DataType::Float4;
                WriteLine("float4 {} = mul({}, {});", expr->identifier,
                          lhs->identifier, rhs->identifier);
            } else {
                expr->dataType = lhs->dataType;
                WriteLine("{} {} = {} {} {};", lhs->dataType, expr->identifier,
                          lhs->identifier, expr->opcode, rhs->identifier);
            }
            break;
        }
        case OpCode::Assignment: {
            DASSERT(expr->arguments.size() == 2);
            const AstNode* lhs = NodeFromAddress(expr->arguments[0]);
            const AstNode* rhs = NodeFromAddress(expr->arguments[1]);
            WriteLine("{} = {};", lhs->identifier, rhs->identifier);
            break;
        }
        case OpCode::Call: {
            // TODO: Implement
            NOT_IMPLEMENTED();
        }
        case OpCode::ConstructFloat2: {
            DASSERT(expr->arguments.size() == 3);
            const AstNode* var = NodeFromAddress(expr->arguments[0]);
            const AstNode* x = NodeFromAddress(expr->arguments[1]);
            const AstNode* y = NodeFromAddress(expr->arguments[2]);
            WriteLine("{} = float2({}, {});", var->identifier, x->identifier,
                      y->identifier);
            break;
        }
        case OpCode::ConstructFloat3: {
            switch (expr->arguments.size()) {
                case 3: {
                    const AstNode* var = NodeFromAddress(expr->arguments[0]);
                    const AstNode* vec2 = NodeFromAddress(expr->arguments[1]);
                    const AstNode* z = NodeFromAddress(expr->arguments[2]);
                    WriteLine("{} = float3({}, {});", var->identifier,
                              vec2->identifier, z->identifier);
                    break;
                }
                case 4: {
                    const AstNode* var = NodeFromAddress(expr->arguments[0]);
                    const AstNode* x = NodeFromAddress(expr->arguments[1]);
                    const AstNode* y = NodeFromAddress(expr->arguments[2]);
                    const AstNode* z = NodeFromAddress(expr->arguments[3]);
                    WriteLine("{} = float4({}, {}, {});", var->identifier,
                              x->identifier, y->identifier, z->identifier);
                    break;
                }
                default: {
                    DASSERT_F(false, "Unknown arguments for op {}", opcode);
                }
            }
            break;
        }
        case OpCode::ConstructFloat4: {
            switch (expr->arguments.size()) {
                case 3: {
                    const AstNode* var = NodeFromAddress(expr->arguments[0]);
                    const AstNode* vec3 = NodeFromAddress(expr->arguments[1]);
                    const AstNode* w = NodeFromAddress(expr->arguments[2]);
                    WriteLine("{} = float4({}, {});", var->identifier,
                              vec3->identifier, w->identifier);
                    break;
                }
                case 4: {
                    const AstNode* var = NodeFromAddress(expr->arguments[0]);
                    const AstNode* vec2 = NodeFromAddress(expr->arguments[1]);
                    const AstNode* z = NodeFromAddress(expr->arguments[2]);
                    const AstNode* w = NodeFromAddress(expr->arguments[3]);
                    WriteLine("{} = float4({}, {}, {});", var->identifier,
                              vec2->identifier, z->identifier, w->identifier);
                    break;
                }
                case 5: {
                    const AstNode* var = NodeFromAddress(expr->arguments[0]);
                    const AstNode* x = NodeFromAddress(expr->arguments[1]);
                    const AstNode* y = NodeFromAddress(expr->arguments[2]);
                    const AstNode* z = NodeFromAddress(expr->arguments[3]);
                    const AstNode* w = NodeFromAddress(expr->arguments[4]);
                    WriteLine("{} = float4({}, {}, {}, {});", var->identifier,
                              x->identifier, y->identifier, z->identifier,
                              w->identifier);
                    break;
                }
                default: {
                    DASSERT_F(false, "Unknown arguments for op {}", opcode);
                }
            }
            break;
        }
        case OpCode::FieldAccess: {
            DASSERT(expr->arguments.size() == 2);
            ValidateIdentifier(expr);
            const AstNode* base = NodeFromAddress(expr->arguments[0]);
            const AstNode* index = NodeFromAddress(expr->arguments[1]);
            const DataType resultType = GetComponentType(base);
            const std::string component = ComponentFromLiteral(index);
            expr->dataType = resultType;
            WriteLine("{} {} = {}.{};", resultType, expr->identifier,
                      base->identifier, component);
            break;
        }
        case OpCode::SampleTexture: {
            DASSERT(expr->arguments.size() == 3);
            ValidateIdentifier(expr);
            const AstNode* tex = NodeFromAddress(expr->arguments[0]);
            const AstNode* sampler = NodeFromAddress(expr->arguments[1]);
            const AstNode* texcoords = NodeFromAddress(expr->arguments[2]);
            expr->dataType = DataType::Float4;
            WriteLine("float4 {} = {}.Sample({}, {});", expr->identifier,
                      tex->identifier, sampler->identifier,
                      texcoords->identifier);
            break;
        }
        default: {
            DASSERT_F(false, "OpCode {} not implemented", expr->opcode);
        }
    }
}

std::string HLSLCodeGenerator::WriteStruct(
    const std::string& type,
    const std::vector<Variable*>& fields) {

    WriteLine("struct {} {}", type, '{');
    PushIndent();
    for (Variable* field : fields) {
        WriteVarDeclaration(field, true, kFieldPrefix);
    }
    PopIndent();
    WriteLine("{};", '}');
    return type;
}

std::string HLSLCodeGenerator::GetHLSLRegisterPrefix(const Variable* var) {
    switch (var->dataType) {
        case DataType::Texture2D: return "t";
        case DataType::Sampler: return "s";
        default: return "b";
    }
}

void HLSLCodeGenerator::WriteVarDeclaration(Variable* var,
                                            bool close,
                                            std::string_view prefix) {
    ValidateIdentifier(var, prefix);
    WriteIndent();
    // [type] [identifier]<:> <semantic> <bind_index>;
    Write("{} {}", var->dataType, var->identifier);
    const bool hasSuffixAttributes =
        var->semantic != Semantic::None || var->bindIdx != kInvalidBindIndex;
    if (hasSuffixAttributes) {
        Write(" : ");
        if (var->semantic != Semantic::None) {
            Write("{}", d3d12::SemanticString(var->semantic));
        } else if (var->bindIdx != kInvalidBindIndex) {
            Write("register({}{})", GetHLSLRegisterPrefix(var), var->bindIdx);
        }
    }
    if (close) {
        Write(";\n");
    }
}

void HLSLCodeGenerator::WriteLiteral(Literal* lit) {
    // [type] [identifier] = [value];
    std::string value;
    switch (lit->dataType) {
        case DataType::Uint: {
            value = std::to_string(lit->GetUint());
            break;
        }
        case DataType::Int: {
            value = std::to_string(lit->GetInt());
            break;
        }
        case DataType::Float: {
            value = std::to_string(lit->GetFloat());
            break;
        }
        default: {
            DASSERT_F(false, "Unknown literal type {}", lit->dataType);
        }
    }
    ValidateIdentifier(lit);
    WriteLine("{} {} = {};", lit->dataType, lit->identifier, value);
}

}  // namespace gpu::internal