#include "program_builder.h"
#include "parser.h"
#include "program.h"

namespace wgsl {

ProgramBuilder::ProgramBuilder() : program_(new Program()) {}

ProgramBuilder::~ProgramBuilder() = default;

void ProgramBuilder::Build(std::string_view code) {
    program_->sourceCode_ = std::string(code);
    code = program_->sourceCode_;
    currentScope_ = program_->globalScope_;
    program_->CreateBuiltinTypes();
    auto parser = Parser(code, this);
    parser_ = &parser;
    parser.Parse();
}

std::unique_ptr<Program> ProgramBuilder::Finalize() {
    // TODO: Do whole program optimizations
    return std::move(program_);
}

void ProgramBuilder::PushGlobalDecl(ast::Variable* var) {
    program_->globalScope_->variables.push_back(var);
}

bool ProgramBuilder::ShouldStopParsing() {
    return stopParsing_;
}

Expected<ConstVariable*> ProgramBuilder::CreateConstVar(
    SourceLoc loc,
    Ident ident,
    const std::optional<TypeInfo>& typeInfo,
    Expression* initializer) {
    // 1. The result type is a result type of the initializer
    // 2. The result type is a user specified type
    // 3. Both are present, check if types are convertible
    if (!initializer) {
        AddError(loc, ErrorCode::ConstDeclNoInitializer);
        return std::unexpected(Result::Error);
    }
    // Result type of the variable [struct | builtin]
    ast::Type* resultType = nullptr;
    // Validate user defined type
    if (typeInfo) {
        // TODO: Unwrap templates and lookup the mangled symbol
        // If not found, declare
        DASSERT_F(typeInfo->templateList.empty(),
                  "Templated types not implemented");
        const auto typeName = typeInfo->ident.name;
        ast::Node* decl = currentScope_->FindSymbol(std::string(typeName));
        if (!decl) {
            AddError(typeInfo->ident.loc, ErrorCode::TypeNotDefined,
                     "type '{}' is not defined", typeName);
            return std::unexpected(Result::Error);
        }
        if (auto* t = decl->As<ast::Type>()) {
            resultType = t;
        } else {
            // Symbol exists but does not refer to a type
            AddError(typeInfo->ident.loc, ErrorCode::IdentNotType,
                     "identifier is not a type name");
            return std::unexpected(Result::Error);
        }
    }
    // Validate initializer expression type
    // Types should be the same or builtin and convertible:
    //   const a : i32 = (AbstrFloat) 3.4;
    //   const a : i32 = (i32) 3i;
    //   const a : i32 = (AbstrInt) 3 * 10;
    ast::Type* initType = initializer->type;
    DASSERT(initType);
    if (resultType) {
        // Assert types are convertible
        if (initType != resultType &&
            !initType->AutoConvertibleTo(resultType)) {
            const SourceLoc exprLoc = initializer->GetLoc();
            // Types are not convertible
            AddError(exprLoc, ErrorCode::TypeError,
                     "this expression cannot be assigned to a const "
                     "value of type {}",
                     to_string(resultType->kind));
            return std::unexpected(Result::Error);
        }
    } else {
        // No explicit type specified
        // Result type equals initializer type
        resultType = initType;
    }
    // Check identifier
    if (currentScope_->FindSymbol(std::string(ident.name))) {
        AddError(ident.loc, ErrorCode::SymbolAlreadyDefined,
                 "symbol '{}' already defined in the current scope",
                 ident.name);
        return std::unexpected(Result::Error);
    }
    auto* decl = program_->alloc_.Allocate<ConstVariable>(
        loc, ident.name, resultType, initializer);
    currentScope_->InsertSymbol(ident.name, decl);
    LOG_INFO("WGSL: Created ConstVariable node. ident: {}, type: {}", ident.name,
             decl->type->kind);
    return decl;
}

Expected<VarVariable*> ProgramBuilder::CreateVar(
    SourceLoc loc,
    Ident ident,
    const std::optional<TemplateList>& varTemplate,
    const std::optional<TypeInfo>& typeSpecifier,
    const std::vector<ast::Attribute*>& attributes,
    Expression* initializer) {
    // Check template parameters
    // Check symbols
    // Check types

    return program_->alloc_.Allocate<VarVariable>(loc, ident.name, nullptr,
                                                  attributes, initializer);
}

Expected<BinaryExpression*> ProgramBuilder::CreateBinaryExpr(
    SourceLoc loc,
    Expression* lhs,
    BinaryExpression::OpCode op,
    Expression* rhs) {
    // Validate types
    ast::Type* resultType = lhs->type;
    if(lhs->type != rhs->type) {
        const auto lr = lhs->type->GetConversionRankTo(rhs->type);
        const auto rl = rhs->type->GetConversionRankTo(lhs->type);
        if (rl > lr) {
            resultType = rhs->type;
        }
    }
    return program_->alloc_.Allocate<BinaryExpression>(loc, resultType, lhs, op,
                                                       rhs);
}

Expected<UnaryExpression*> ProgramBuilder::CreateUnaryExpr(
    SourceLoc loc,
    UnaryExpression::OpCode op,
    Expression* rhs) {
    // Validate types
    // Validate symbols
    return program_->alloc_.Allocate<UnaryExpression>(loc, rhs->type, op, rhs);
}

Expected<IdentExpression*> ProgramBuilder::CreateIdentExpr(const Ident& ident) {
    // Check name
    ast::Node* symbol = currentScope_->FindSymbol(ident.name);
    if(!symbol) {
        AddError(ident.loc, ErrorCode::SymbolNotFound,
                 "symbol '{}' not found in the current scope", ident.name);
        return std::unexpected(Result::Error);
    }
    if(symbol && !symbol->Is<ast::Variable>()) {
        AddError(ident.loc, ErrorCode::SymbolNotVariable,
                 "symbol '{}' is not a variable", ident.name);
        return std::unexpected(Result::Error);
    }
    auto* var = symbol->As<ast::Variable>();
    return program_->alloc_.Allocate<IdentExpression>(ident.loc, var, var->type);
}

Expected<IntLiteralExpression*> ProgramBuilder::CreateIntLiteralExpr(
    SourceLoc loc,
    int64_t value,
    Type::Kind type) {
    ast::Node* typeNode = currentScope_->FindSymbol(to_string(type));
    DASSERT(typeNode && typeNode->Is<ast::Type>());
    return program_->alloc_.Allocate<IntLiteralExpression>(
        loc, typeNode->As<ast::Type>(), value);
}

Expected<FloatLiteralExpression*> ProgramBuilder::CreateFloatLiteralExpr(
    SourceLoc loc,
    double value,
    Type::Kind type) {
    ast::Node* typeNode = currentScope_->FindSymbol(to_string(type));
    DASSERT(typeNode && typeNode->Is<ast::Type>());
    return program_->alloc_.Allocate<FloatLiteralExpression>(
        loc, typeNode->As<ast::Type>(), value);
}

Expected<BoolLiteralExpression*> ProgramBuilder::CreateBoolLiteralExpr(
    SourceLoc loc,
    bool value) {
    ast::Node* typeNode = currentScope_->FindSymbol(to_string(Type::Kind::Bool));
    DASSERT(typeNode && typeNode->Is<ast::Type>());
    return program_->alloc_.Allocate<BoolLiteralExpression>(loc, nullptr,
                                                            value);
}

Expected<ast::Attribute*> ProgramBuilder::CreateAttribute(
    SourceLoc loc,
    wgsl::AttributeName attr,
    Expression* expr) {
    // Validate expression
    return program_->alloc_.Allocate<ast::Attribute>(loc, attr, expr);
}

void ProgramBuilder::FormatAndAddMsg(SourceLoc loc,
                                     ErrorCode code,
                                     const std::string& msg) {
    // Break on 'unimplemented' messages
    DASSERT(code != ErrorCode::Unimplemented);
    // Whether the loc points to an empty line
    // So instead print the previous non empty line
    auto [sourceLine, isLocOnEmpty] = [&] {
        std::string_view line = parser_->GetLine(loc.line);
        if(!line.empty()) {
            return std::make_pair(line, false);
        }
        uint32_t prevLine = loc.line - 1;
        // Source text is empty
        // Lines are 1 based
        if(prevLine == 0) {
            return std::make_pair(line, false);
        }
        for (; prevLine > 0 && line.empty(); --prevLine) {
            line = parser_->GetLine(prevLine);
        }
        return std::make_pair(line, true);
    }();
    // Trim line leading whitespaces
    uint32_t leadSpaceLen = 0;
    if(!isLocOnEmpty) {
        while (!sourceLine.empty() && ASCII::IsSpace(sourceLine.front())) {
            sourceLine = sourceLine.substr(1);
            ++leadSpaceLen;
        }
    }
    // Generate the "^^^" pointing to the error pos. I.e.
    // var my_var = ; 
    //             ^ expected an expression
    std::string msgLine;
    for (uint32_t i = 0; i < loc.col - 1 - leadSpaceLen; ++i) {
        msgLine.push_back(' ');
    }
    // ^^^
    for (uint32_t i = 0; i < loc.len; ++i) {
        msgLine.push_back('^');
    }
    msgLine.push_back(' ');
    msgLine.append(msg);
    // Combine
    const auto lineNum = std::format("{}", loc.line);
    std::string result;
    // clang-format off
    result.append("\n");
    result.append(" ").append(lineNum.size(), ' ').append(" | \n");
    result.append(" ").append(lineNum            ).append(" | ").append(sourceLine).append("\n");
    result.append(" ").append(lineNum.size(), ' ').append(" | ").append(msgLine).append("\n");
    result.append(" ").append(lineNum.size(), ' ').append(" | \n");
    // clang-format on
    program_->diags_.push_back(
        {Program::DiagMsg::Type::Error, loc, code, result});
    stopParsing_ = true;
}

}  // namespace wgsl
