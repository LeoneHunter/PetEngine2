#include "program_builder.h"
#include "builtin_op.h"
#include "parser.h"
#include "program.h"

namespace wgsl {

ProgramBuilder::ProgramBuilder() : program_(new Program()) {}

ProgramBuilder::~ProgramBuilder() {}

void ProgramBuilder::CreateBuiltinSymbols() {
    Program::Scope& scope = *program_->globalScope_;
    BumpAllocator& alloc = program_->alloc_;

    const auto makeScalarTypeSymbol = [&](ast::Type::Kind kind) {
        auto* type =
            alloc.Allocate<ast::Type>(SourceLoc(), kind, to_string(kind));
        scope.InsertSymbol(to_string(kind), type);
    };
    makeScalarTypeSymbol(ast::Type::Kind::AbstrFloat);
    makeScalarTypeSymbol(ast::Type::Kind::AbstrInt);
    makeScalarTypeSymbol(ast::Type::Kind::U32);
    makeScalarTypeSymbol(ast::Type::Kind::I32);
    makeScalarTypeSymbol(ast::Type::Kind::F32);
    makeScalarTypeSymbol(ast::Type::Kind::Bool);

    ast::Type* boolType = currentScope_->FindType("bool");

    scope.InsertSymbol(GetBuiltinOpName(OpCode::Negation),
                       alloc.Allocate<UnaryArithmeticOp>());
    scope.InsertSymbol(GetBuiltinOpName(OpCode::Negation),
                       alloc.Allocate<UnaryArithmeticOp>());
    scope.InsertSymbol(GetBuiltinOpName(OpCode::BitNot),
                       alloc.Allocate<UnaryBitwiseOp>());
    scope.InsertSymbol(GetBuiltinOpName(OpCode::LogNot),
                       alloc.Allocate<UnaryLogicalOp>(boolType));

    scope.InsertSymbol(GetBuiltinOpName(OpCode::Mul),
                       alloc.Allocate<BinaryArithmeticOp>(OpCode::Mul));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::Div),
                       alloc.Allocate<BinaryArithmeticOp>(OpCode::Div));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::Add),
                       alloc.Allocate<BinaryArithmeticOp>(OpCode::Add));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::Sub),
                       alloc.Allocate<BinaryArithmeticOp>(OpCode::Sub));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::Mod),
                       alloc.Allocate<BinaryArithmeticOp>(OpCode::Mod));

    scope.InsertSymbol(
        GetBuiltinOpName(OpCode::LogAnd),
        alloc.Allocate<BinaryLogicalOp>(OpCode::LogAnd, boolType));
    scope.InsertSymbol(
        GetBuiltinOpName(OpCode::LogOr),
        alloc.Allocate<BinaryLogicalOp>(OpCode::LogOr, boolType));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::Less),
                       alloc.Allocate<BinaryLogicalOp>(OpCode::Less, boolType));
    scope.InsertSymbol(
        GetBuiltinOpName(OpCode::Greater),
        alloc.Allocate<BinaryLogicalOp>(OpCode::Greater, boolType));
    scope.InsertSymbol(
        GetBuiltinOpName(OpCode::LessEqual),
        alloc.Allocate<BinaryLogicalOp>(OpCode::LessEqual, boolType));
    scope.InsertSymbol(
        GetBuiltinOpName(OpCode::GreaterEqual),
        alloc.Allocate<BinaryLogicalOp>(OpCode::GreaterEqual, boolType));
    scope.InsertSymbol(
        GetBuiltinOpName(OpCode::Equal),
        alloc.Allocate<BinaryLogicalOp>(OpCode::Equal, boolType));
    scope.InsertSymbol(
        GetBuiltinOpName(OpCode::NotEqual),
        alloc.Allocate<BinaryLogicalOp>(OpCode::NotEqual, boolType));

    scope.InsertSymbol(GetBuiltinOpName(OpCode::BitAnd),
                       alloc.Allocate<BinaryBitwiseOp>(OpCode::BitAnd));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::BitOr),
                       alloc.Allocate<BinaryBitwiseOp>(OpCode::BitOr));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::BitXor),
                       alloc.Allocate<BinaryBitwiseOp>(OpCode::BitXor));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::BitLsh),
                       alloc.Allocate<BinaryBitwiseOp>(OpCode::BitLsh));
    scope.InsertSymbol(GetBuiltinOpName(OpCode::BitRsh),
                       alloc.Allocate<BinaryBitwiseOp>(OpCode::BitRsh));
}

void ProgramBuilder::Build(std::string_view code) {
    // Copy source code
    program_->sourceCode_ = std::string(code);
    code = program_->sourceCode_;
    currentScope_ = program_->globalScope_;
    CreateBuiltinSymbols();

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
        return std::unexpected(ErrorCode::Error);
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
            return std::unexpected(ErrorCode::Error);
        }
        if (auto* t = decl->As<ast::Type>()) {
            resultType = t;
        } else {
            // Symbol exists but does not refer to a type
            AddError(typeInfo->ident.loc, ErrorCode::IdentNotType,
                     "identifier is not a type name");
            return std::unexpected(ErrorCode::Error);
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
            return std::unexpected(ErrorCode::Error);
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
        return std::unexpected(ErrorCode::Error);
    }
    auto* decl = program_->alloc_.Allocate<ConstVariable>(
        loc, ident.name, resultType, initializer);
    currentScope_->InsertSymbol(ident.name, decl);
    LOG_INFO("WGSL: Created ConstVariable node. ident: {}, type: {}",
             ident.name, decl->type->kind);
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

Expected<Expression*> ProgramBuilder::CreateBinaryExpr(SourceLoc loc,
                                                       Expression* lhs,
                                                       OpCode op,
                                                       Expression* rhs) {
    auto* symbol = currentScope_->FindSymbol(GetBuiltinOpName(op));
    DASSERT(symbol);
    BuiltinOp* handler = symbol->As<BuiltinOp>();
    DASSERT(handler);

    OpResult result = handler->CheckAndEval(lhs, rhs);
    // Check errors
    if (result.err == ErrorCode::InvalidArgs) {
        AddError(loc, ErrorCode::InvalidArgs,
                 "no operator {} can accept all arguments. Arguments types are "
                 "{}, {}",
                 to_string(op), lhs->type->name, rhs->type->name);
        return std::unexpected(ErrorCode::Error);
    }
    if (result.err == ErrorCode::ConstOverflow) {
        AddError(loc, ErrorCode::ConstOverflow);
        return std::unexpected(ErrorCode::Error);
    }
    // If const evaluated, replace the result expression with a literal
    if (result.value) {
        if (result.returnType->IsInteger()) {
            return program_->alloc_.Allocate<ast::IntLiteralExpression>(
                loc, result.returnType, std::get<int64_t>(*result.value));
        }
        if (result.returnType->IsFloat()) {
            return program_->alloc_.Allocate<ast::FloatLiteralExpression>(
                loc, result.returnType, std::get<double>(*result.value));
        }
        return program_->alloc_.Allocate<ast::BoolLiteralExpression>(
            loc, result.returnType, std::get<bool>(*result.value));
    }
    return program_->alloc_.Allocate<ast::BinaryExpression>(
        loc, result.returnType, lhs, op, rhs);
}

Expected<Expression*> ProgramBuilder::CreateUnaryExpr(SourceLoc loc,
                                                      OpCode op,
                                                      Expression* rhs) {
    auto* symbol = currentScope_->FindSymbol(GetBuiltinOpName(op));
    DASSERT(symbol);
    BuiltinOp* handler = symbol->As<BuiltinOp>();
    DASSERT(handler);

    OpResult result = handler->CheckAndEval(rhs);
    // Check errors
    if (result.err == ErrorCode::InvalidArgs) {
        AddError(loc, ErrorCode::InvalidArgs,
                 "no operator {} can accept the argument of type {}",
                 to_string(op), rhs->type->name);
        return std::unexpected(ErrorCode::Error);
    }
    if (result.err == ErrorCode::ConstOverflow) {
        AddError(loc, ErrorCode::ConstOverflow);
        return std::unexpected(ErrorCode::Error);
    }
    // If const evaluated, replace the result expression with a literal
    if (result.value) {
        if (result.returnType->IsInteger()) {
            return program_->alloc_.Allocate<ast::IntLiteralExpression>(
                loc, result.returnType, std::get<int64_t>(*result.value));
        }
        if (result.returnType->IsFloat()) {
            return program_->alloc_.Allocate<ast::FloatLiteralExpression>(
                loc, result.returnType, std::get<double>(*result.value));
        }
        return program_->alloc_.Allocate<ast::BoolLiteralExpression>(
            loc, result.returnType, std::get<bool>(*result.value));
    }
    return program_->alloc_.Allocate<UnaryExpression>(loc, result.returnType,
                                                      op, rhs);
}

Expected<IdentExpression*> ProgramBuilder::CreateIdentExpr(const Ident& ident) {
    // Check name
    ast::Node* symbol = currentScope_->FindSymbol(ident.name);
    if (!symbol) {
        AddError(ident.loc, ErrorCode::SymbolNotFound,
                 "symbol '{}' not found in the current scope", ident.name);
        return std::unexpected(ErrorCode::Error);
    }
    if (symbol && !symbol->Is<ast::Variable>()) {
        AddError(ident.loc, ErrorCode::SymbolNotVariable,
                 "symbol '{}' is not a variable", ident.name);
        return std::unexpected(ErrorCode::Error);
    }
    auto* var = symbol->As<ast::Variable>();
    return program_->alloc_.Allocate<IdentExpression>(ident.loc, var,
                                                      var->type);
}

Expected<IntLiteralExpression*> ProgramBuilder::CreateIntLiteralExpr(
    SourceLoc loc,
    int64_t value,
    Type::Kind type) {
    ast::Node* typeNode = currentScope_->FindSymbol(to_string(type));
    DASSERT(typeNode && typeNode->Is<ast::Type>());
    // Check for overflow for result type
    if (type == Type::Kind::U32) {
        if (value > std::numeric_limits<uint32_t>::max()) {
            AddError(loc, ErrorCode::LiteralInitValueTooLarge);
            return std::unexpected(ErrorCode::Error);
        }
    } else {
        // AbstractInt || i32
        if (value > std::numeric_limits<int32_t>::max()) {
            AddError(loc, ErrorCode::LiteralInitValueTooLarge);
            return std::unexpected(ErrorCode::Error);
        }
    }
    return program_->alloc_.Allocate<IntLiteralExpression>(
        loc, typeNode->As<ast::Type>(), value);
}

Expected<FloatLiteralExpression*> ProgramBuilder::CreateFloatLiteralExpr(
    SourceLoc loc,
    double value,
    Type::Kind type) {
    ast::Node* typeNode = currentScope_->FindSymbol(to_string(type));
    DASSERT(typeNode && typeNode->Is<ast::Type>());
    // Check for overflow for result type
    if (type == Type::Kind::F32 || type == Type::Kind::AbstrFloat) {
        if (value > std::numeric_limits<float>::max()) {
            AddError(loc, ErrorCode::LiteralInitValueTooLarge);
            return std::unexpected(ErrorCode::Error);
        }
    } else {
        AddError(loc, ErrorCode::Unimplemented,
                 "half types are not implemented");
        return std::unexpected(ErrorCode::Error);
    }
    return program_->alloc_.Allocate<FloatLiteralExpression>(
        loc, typeNode->As<ast::Type>(), value);
}

Expected<BoolLiteralExpression*> ProgramBuilder::CreateBoolLiteralExpr(
    SourceLoc loc,
    bool value) {
    ast::Type* type = currentScope_->FindType(to_string(Type::Kind::Bool));
    DASSERT(type);
    return program_->alloc_.Allocate<BoolLiteralExpression>(loc, type, value);
}

Expected<ast::Attribute*> ProgramBuilder::CreateAttribute(
    SourceLoc loc,
    wgsl::AttributeName attr,
    Expression* expr) {
    // Validate expression
    return program_->alloc_.Allocate<ast::Attribute>(loc, attr, expr);
}

std::optional<int64_t> ProgramBuilder::TryGetConstInt(ast::Node* node) {
    DASSERT(node);
    if (auto ilit = node->As<IntLiteralExpression>()) {
        return ilit->value;
    }
    if (auto ilit = node->As<BoolLiteralExpression>()) {
        return ilit->value;
    }
    if (auto ident = node->As<IdentExpression>()) {
        if (auto cvar = ident->decl->As<ConstVariable>()) {
            if (auto lit = cvar->initializer->As<IntLiteralExpression>()) {
                return lit->value;
            }
            if (auto lit = cvar->initializer->As<BoolLiteralExpression>()) {
                return lit->value;
            }
        }
    }
    return std::nullopt;
}

std::optional<double> ProgramBuilder::ReadConstValueDouble(ast::Node* node) {
    DASSERT(node);
    // 3.4 | 3.0f -> 3.4, 3.0
    if (auto e = node->As<FloatLiteralExpression>()) {
        return e->value;
    }
    // 23 -> 23.0f
    if (auto e = node->As<IntLiteralExpression>()) {
        if (e->type->IsAbstract()) {
            return (double)e->value;
        }
    }
    // ident a -> const a -> a.initializer
    if (auto ident = node->As<IdentExpression>()) {
        if (auto cvar = ident->decl->As<ConstVariable>()) {
            if (auto lit = cvar->initializer->As<IntLiteralExpression>()) {
                return (double)lit->value;
            }
            if (auto lit = cvar->initializer->As<FloatLiteralExpression>()) {
                return (double)lit->value;
            }
        }
    }
    return std::nullopt;
}

bool ProgramBuilder::IsNodeConst(ast::Node* node) {
    DASSERT(node);
    if (node->Is<LiteralExpression>()) {
        return true;
    }
    if (auto* ident = node->As<IdentExpression>()) {
        DASSERT(ident->decl);
        if (ident->decl->Is<ConstVariable>()) {
            return true;
        }
    }
    return false;
}

Expected<ast::Type*> ProgramBuilder::TypeCheckAndConvert(ast::Expression* lhs,
                                                         ast::Expression* rhs) {
    // Check binary op. I.e. "a + b", "7 + 1", "1.0f + 1"
    DASSERT(lhs->type && rhs->type);
    if (lhs->type == rhs->type) {
        return lhs->type;
    }
    // Concrete
    constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
    const auto lr = lhs->type->GetConversionRankTo(rhs->type);
    const auto rl = rhs->type->GetConversionRankTo(lhs->type);
    // Not convertible to each other
    if (lr == kMax && rl == kMax) {
        // TODO: Generate more detailed error for mismatched types
        return std::unexpected(ErrorCode::Error);
    }
    // Find common type
    auto* resultType = lhs->type;
    if (rl > lr) {
        resultType = rhs->type;
    }
    return resultType;
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
        if (!line.empty()) {
            return std::make_pair(line, false);
        }
        uint32_t prevLine = loc.line - 1;
        // Source text is empty
        // Lines are 1 based
        if (prevLine == 0) {
            return std::make_pair(line, false);
        }
        for (; prevLine > 0 && line.empty(); --prevLine) {
            line = parser_->GetLine(prevLine);
        }
        return std::make_pair(line, true);
    }();
    // Trim line leading whitespaces
    uint32_t leadSpaceLen = 0;
    if (!isLocOnEmpty) {
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
