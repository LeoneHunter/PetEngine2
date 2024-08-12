#include "hlsl_codegen.h"

namespace gpu::internal {

std::unique_ptr<ShaderCode> HLSLCodeGenerator::Generate(
    internal::Scope* root,
    CodeGenerator::Delegate* ast) {
    //
    ast_ = ast;
    std::stringstream code;
    stream_ = &code;
    ParseGlobal(root);
    return std::make_unique<ShaderCode>(code.str());
}

void HLSLCodeGenerator::ParseGlobal(Scope* root) {
    for (Address addr : root->children) {
        if (Variable* var = NodeFromAddress<Variable>(addr)) {
            WriteVarDeclaration(var);
            continue;
        } else if (Function* func = NodeFromAddress<Function>(addr)) {
            ParseFunction(func);
            continue;
        }
        DASSERT_M(false, "Unknown node type encountered");
    }
}

void HLSLCodeGenerator::ParseFunction(Function* func) {
    isInsideFunction_ = true;
    // Gather inputs, outputs and uniforms
    std::vector<Variable*> inputs;
    std::vector<Variable*> outputs;
    std::vector<Expression*> body;
    // Gather inputs and outputs
    for (Address addr : func->children) {
        if (Variable* var = NodeFromAddress<Variable>(addr)) {
            switch (var->attr) {
                case Attribute::Input: {
                    inputs.emplace_back(var);
                    break;
                }
                case Attribute::Output: {
                    outputs.emplace_back(var);
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
        case 0:
            returnType = "void";
            break;
        case 1:
            returnType = to_string(outputs.back()->dataType);
            break;
        default:
            returnType = CreateClassIdentifier();
    }
    // Write outputs struct declaration
    if (outputs.size() > 1) {
        WriteStruct(returnType, outputs);
    }
    // Write signature
    const std::string identifier =
        func->identifier.empty() ? CreateFuncIdentifier() : func->identifier;
    Write("{} {}(", returnType, identifier);
    for (size_t i = 0; i < inputs.size(); ++i) {
        Variable* param = inputs[i];
        if (i > 0) {
            Write(", ");
        }
        WriteVarDeclaration(param, false);
    }
    Write(") {}\n", '{');
    PushIndent();
    std::string returnIdentifier;
    // Write returns declaration
    switch (outputs.size()) {
        case 0:
            break;
        case 1: {
            WriteVarDeclaration(outputs.back());
            returnIdentifier = outputs.back()->identifier;
            break;
        }
        default: {
            // TODO: Write outputs var declaration
            // OutputsStruct out;
            // TODO: Update output identifiers to struct.var
            NOT_IMPLEMENTED();
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
        } else {
            NOT_IMPLEMENTED();
        }
    }
    // Write return
    if (outputs.size() > 0) {
        WriteIndent();
        Write("return {};\n", returnIdentifier);
    }
    PopIndent();
    Write("{}\n", '}');
    isInsideFunction_ = false;
    localVarCounter_ = 0;
}

std::string HLSLCodeGenerator::GetArgumentType(const AstNode* node) {
    if (const Literal* literal = CastNode<Literal>(node)) {
        return to_string(literal->dataType);
    } else if (const Variable* var = CastNode<Variable>(node)) {
        return to_string(var->dataType);
    }
    return "";
}

std::string HLSLCodeGenerator::GetComponentType(const AstNode* node) {
    if (const Variable* var = CastNode<Variable>(node)) {
        switch (var->dataType) {
            case DataType::Float2:
            case DataType::Float3:
            case DataType::Float4:
                return "float";
                // TODO: Implement other types
        }
    }
    return "";
}

std::string HLSLCodeGenerator::GetComponent(const AstNode* node) {
    if (const Literal* literal = CastNode<Literal>(node)) {
        if (literal->dataType == DataType::Uint) {
            const int64_t index64 = std::get<int64_t>(literal->value);
            if (index64 <= std::numeric_limits<uint32_t>::max()) {
                const uint32_t index32 = (uint32_t)index64;
                switch (index32) {
                    case 0:
                        return "x";
                    case 1:
                        return "y";
                    case 2:
                        return "z";
                    case 3:
                        return "w";
                }
            }
        }
    }
    return "";
}

void HLSLCodeGenerator::WriteExpression(Expression* expr, Address addr) {
    using OpCode = Expression::OpCode;
    switch (expr->opcode) {
        case OpCode::Add:
        case OpCode::Sub:
        case OpCode::Mul:
        case OpCode::Div: {
            DASSERT(expr->arguments.size() == 2);
            const AstNode* lhs = NodeFromAddress(expr->arguments[0]);
            const AstNode* rhs = NodeFromAddress(expr->arguments[1]);
            ValidateIdentifier(expr);
            WriteLine("{} {} = {} {} {};", GetArgumentType(lhs),
                      expr->identifier, lhs->identifier, expr->opcode,
                      rhs->identifier);
        } break;
        case OpCode::Assign: {
            DASSERT(expr->arguments.size() == 2);
            const AstNode* lhs = NodeFromAddress(expr->arguments[0]);
            const AstNode* rhs = NodeFromAddress(expr->arguments[1]);
            WriteLine("{} = {};", lhs->identifier, rhs->identifier);
        } break;
        case OpCode::FieldAccess: {
            DASSERT(expr->arguments.size() == 2);
            ValidateIdentifier(expr);
            const AstNode* base = NodeFromAddress(expr->arguments[0]);
            const AstNode* index = NodeFromAddress(expr->arguments[1]);
            const std::string resultType = GetComponentType(base);
            const std::string component = GetComponent(index);
            WriteLine("{} {} = {}.{};", resultType, expr->identifier,
                      base->identifier, component);
        } break;
        default: {
            DASSERT_F(false, "OpCode {} not implemented", expr->opcode);
        }
    }
}

std::string HLSLCodeGenerator::WriteStruct(
    const std::string& type,
    const std::vector<Variable*>& fields) {
    //
    WriteLine("struct {} {}", type, '{');
    PushIndent();
    for (Variable* field : fields) {
        WriteVarDeclaration(field);
    }
    PopIndent();
    WriteLine("{}", '}');
    return type;
}

std::string HLSLCodeGenerator::GetHLSLRegisterPrefix(const Variable* var) {
    switch (var->dataType) {
        case DataType::Texture2D:
            return "t";
        case DataType::Sampler:
            return "s";
        default:
            return "b";
    }
}

void HLSLCodeGenerator::WriteVarDeclaration(Variable* var, bool close) {
    ValidateIdentifier(var);
    WriteIndent();
    // [type] [identifier]<:> <semantic> <bind_index>;
    Write("{} {}", var->dataType, var->identifier);
    const bool hasSuffixAttributes =
        var->semantic != Semantic::None || var->bindIdx != kInvalidBindIndex;
    if (hasSuffixAttributes) {
        Write(" : ");
        if (var->semantic != Semantic::None) {
            Write("{}", var->semantic);
        } else if (var->bindIdx != kInvalidBindIndex) {
            Write("register({}{})", GetHLSLRegisterPrefix(var), var->bindIdx);
        }
    }
    if (close) {
        Write(";\n");
    }
}

}  // namespace gpu::internal