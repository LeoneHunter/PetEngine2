#include "program_builder.h"
#include "parser.h"
#include "program.h"

namespace wgsl {

namespace {

template <class T>
constexpr T EvalArithmeticOp(BinaryExpression::OpCode op, T lhs, T rhs) {
    using Op = BinaryExpression::OpCode;
    switch (op) {
        case Op::Add: return lhs + rhs;
        case Op::Sub: return lhs - rhs;
        case Op::Mul: return lhs * rhs;
        case Op::Div: return lhs / rhs;
        default: return {};
    }
}

template <class T>
constexpr T EvalArithmeticOp(UnaryExpression::OpCode op, T rhs) {
    using Op = UnaryExpression::OpCode;
    return op == Op::Minus ? -rhs : rhs;
}

template <class T>
constexpr bool EvalRelationalOp(BinaryExpression::OpCode op, T lhs, T rhs) {
    using Op = BinaryExpression::OpCode;
    switch (op) {
        case Op::LessThan: return lhs < rhs;
        case Op::GreaterThan: return lhs > rhs;
        case Op::LessThanEqual: return lhs <= rhs;
        case Op::GreaterThanEqual: return lhs >= rhs;
        case Op::Equal: return lhs == rhs;
        case Op::NotEqual: return lhs != rhs;
        case Op::And: return lhs && rhs;
        case Op::Or: return lhs || rhs;
        default: return {};
    }
}

constexpr int64_t EvalRelationalOp(UnaryExpression::OpCode op, int64_t rhs) {
    using Op = UnaryExpression::OpCode;
    return op == Op::Negation ? !rhs : rhs;
}

template <class T>
constexpr T EvalBitwiseOp(BinaryExpression::OpCode op, T lhs, T rhs) {
    using Op = BinaryExpression::OpCode;
    switch (op) {
        case Op::BitwiseAnd: return lhs & rhs;
        case Op::BitwiseOr: return lhs | rhs;
        case Op::BitwiseXor: return lhs ^ rhs;
        case Op::BitwiseLeftShift: return lhs << rhs;
        case Op::BitwiseRightShift: return lhs >> rhs;
        default: return {};
    }
}

template <class T>
constexpr T EvalBitwiseOp(UnaryExpression::OpCode op, T rhs) {
    using Op = UnaryExpression::OpCode;
    return op == Op::BitwiseNot ? ~rhs : rhs;
}

}  // namespace

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

Expected<Expression*> ProgramBuilder::CreateBinaryExpr(
    SourceLoc loc,
    Expression* lhs,
    BinaryExpression::OpCode op,
    Expression* rhs) {

    // TODO: Handle templates
    // Validate types
    auto typeRes = TypeCheckAndConvert(lhs, rhs);
    if (!typeRes) {
        AddError(loc, ErrorCode::InvalidArgs);
        return std::unexpected(Result::Error);
    }
    ast::Type* argsType = *typeRes;
    ast::Type* returnType = *typeRes;

    if (BinaryExpression::IsRelational(op)) {
        ast::Type* type = currentScope_->FindType("bool");
        DASSERT(type);
        returnType = type;
    }

    // Validate arguments
    if (BinaryExpression::IsArithmetic(op) && !argsType->IsArithmetic()) {
        AddError(loc, ErrorCode::InvalidArgs,
                 "no operator {} can accept all arguments."
                 " Arguments are {} {}",
                 OpToStringDiag(op), lhs->type->name, rhs->type->name);
        return std::unexpected(Result::Error);
    }
    if (BinaryExpression::IsBitwise(op) && !argsType->IsInteger()) {
        AddError(loc, ErrorCode::InvalidArgs,
                 "no operator {} can accept all arguments."
                 " Arguments are {} {}",
                 OpToStringDiag(op), lhs->type->name, rhs->type->name);
        return std::unexpected(Result::Error);
    }
    // Check if not const
    if (!IsNodeConst(lhs) || !IsNodeConst(rhs)) {
        return program_->alloc_.Allocate<BinaryExpression>(loc, argsType, lhs,
                                                           op, rhs);
    }
    // Evaluate if const
    if (argsType->IsFloat()) {
        std::optional<double> lhsVal = ReadConstValueDouble(lhs);
        std::optional<double> rhsVal = ReadConstValueDouble(rhs);
        DASSERT(lhsVal && rhsVal);

        if (BinaryExpression::IsRelational(op)) {
            const bool res = EvalRelationalOp(op, *lhsVal, *rhsVal);
            return program_->alloc_.Allocate<ast::BoolLiteralExpression>(
                loc, returnType, res);
        }
        if (BinaryExpression::IsArithmetic(op)) {
            const double res = EvalArithmeticOp(op, *lhsVal, *rhsVal);
            // Validate overflow
            if (!argsType->IsAbstract()) {
                if (res > std::numeric_limits<float>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            }
            return program_->alloc_.Allocate<ast::FloatLiteralExpression>(
                loc, returnType, res);
        }
    }
    // Integer
    if (argsType->IsInteger()) {
        std::optional<int64_t> lhsVal = TryGetConstInt(lhs);
        std::optional<int64_t> rhsVal = TryGetConstInt(rhs);
        // Both are const
        DASSERT(lhsVal && rhsVal);

        if (BinaryExpression::IsRelational(op)) {
            const bool res = EvalRelationalOp(op, *lhsVal, *rhsVal);
            return program_->alloc_.Allocate<ast::BoolLiteralExpression>(
                loc, returnType, res);
        }
        if (BinaryExpression::IsArithmetic(op)) {
            const int64_t res = EvalArithmeticOp(op, *lhsVal, *rhsVal);
            // Validate overflow
            if (argsType->kind == Type::Kind::U32) {
                if (res > std::numeric_limits<uint32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            } else if (argsType->kind == Type::Kind::I32) {
                if (res > std::numeric_limits<int32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            }
            return program_->alloc_.Allocate<ast::IntLiteralExpression>(
                loc, returnType, res);
        }
        if (BinaryExpression::IsBitwise(op)) {
            int64_t res = 0;
            if (argsType->IsSigned()) {
                res = EvalBitwiseOp<int64_t>(op, *lhsVal, *rhsVal);
            } else {
                res = static_cast<int64_t>(
                    EvalBitwiseOp<uint64_t>(op, *lhsVal, *rhsVal));
            }
            // Validate overflow
            if (argsType->kind == Type::Kind::U32) {
                if (res > std::numeric_limits<uint32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            } else if (argsType->kind == Type::Kind::I32) {
                if (res > std::numeric_limits<int32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            }
            return program_->alloc_.Allocate<ast::IntLiteralExpression>(
                loc, argsType, res);
        }
    }
    // Both argumetns are bool
    if (argsType->IsBool()) {
        std::optional<int64_t> lhsVal = TryGetConstInt(lhs);
        std::optional<int64_t> rhsVal = TryGetConstInt(rhs);
        // Both are const
        DASSERT(lhsVal && rhsVal);

        if (BinaryExpression::IsRelational(op)) {
            const bool res = EvalRelationalOp(op, *lhsVal, *rhsVal);
            return program_->alloc_.Allocate<ast::BoolLiteralExpression>(
                loc, returnType, res);
        }
    }
    UNREACHABLE();
    return std::unexpected(Result::Error);
}

Expected<Expression*> ProgramBuilder::CreateUnaryExpr(
    SourceLoc loc,
    UnaryExpression::OpCode op,
    Expression* rhs) {

    // TODO: Handle templates
    ast::Type* argType = rhs->type;
    ast::Type* returnType = argType;

    if (UnaryExpression::IsRelational(op)) {
        ast::Type* type = currentScope_->FindType("bool");
        DASSERT(type);
        returnType = type;

        if (!argType->IsBool()) {
            AddError(loc, ErrorCode::InvalidArgs,
                     "no operator {} can accept the argument."
                     " Argument is {}",
                     OpToStringDiag(op), rhs->type->name);
            return std::unexpected(Result::Error);
        }
    }
    // Validate arguments
    // TODO: Explicitly-typed unsigned integer literal cannot be negated.
    // var u32_neg = -1u; // invalid: unary minus does not support u32
    if (UnaryExpression::IsArithmetic(op) && !argType->IsArithmetic()) {
        AddError(loc, ErrorCode::InvalidArgs,
                 "no operator {} can accept the argument."
                 " Argument is {}",
                 OpToStringDiag(op), rhs->type->name);
        return std::unexpected(Result::Error);
    }
    if (UnaryExpression::IsBitwise(op) && !argType->IsInteger()) {
        AddError(loc, ErrorCode::InvalidArgs,
                 "no operator {} can accept the argument."
                 " Argument is {}",
                 OpToStringDiag(op), rhs->type->name);
        return std::unexpected(Result::Error);
    }
    // Check if not const
    if (!IsNodeConst(rhs)) {
        return program_->alloc_.Allocate<UnaryExpression>(loc, returnType, op,
                                                          rhs);
    }
    // Evaluate if const
    if (argType->IsFloat()) {
        std::optional<double> rhsVal = ReadConstValueDouble(rhs);
        DASSERT(rhsVal);

        if (UnaryExpression::IsRelational(op)) {
            const bool res = EvalRelationalOp(op, *rhsVal);
            return program_->alloc_.Allocate<ast::BoolLiteralExpression>(
                loc, returnType, res);
        }
        if (UnaryExpression::IsArithmetic(op)) {
            const double res = EvalArithmeticOp(op, *rhsVal);
            // Validate overflow
            if (!argType->IsAbstract()) {
                if (res > std::numeric_limits<float>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            }
            return program_->alloc_.Allocate<ast::FloatLiteralExpression>(
                loc, argType, res);
        }
    }
    // Integer types
    if (argType->IsInteger()) {
        std::optional<int64_t> rhsVal = TryGetConstInt(rhs);
        DASSERT(rhsVal);

        if (UnaryExpression::IsRelational(op)) {
            const bool res = EvalRelationalOp(op, *rhsVal);
            return program_->alloc_.Allocate<ast::BoolLiteralExpression>(
                loc, returnType, res);
        }
        if (UnaryExpression::IsArithmetic(op)) {
            const int64_t res = EvalArithmeticOp(op, *rhsVal);
            // Validate overflow
            if (argType->kind == Type::Kind::U32) {
                if (res > std::numeric_limits<uint32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            } else if (argType->kind == Type::Kind::I32) {
                if (res > std::numeric_limits<int32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            }
            return program_->alloc_.Allocate<ast::IntLiteralExpression>(
                loc, argType, res);
        }
        if (UnaryExpression::IsBitwise(op)) {
            int64_t res = 0;
            if (argType->IsSigned()) {
                res = EvalBitwiseOp<int64_t>(op, *rhsVal);
            } else {
                if(*rhsVal > std::numeric_limits<uint32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
                const auto val32 = static_cast<uint32_t>(*rhsVal);
                res = static_cast<int64_t>(EvalBitwiseOp<uint32_t>(op, val32));
            }
            // Validate overflow
            if (argType->kind == Type::Kind::U32) {
                if (res > std::numeric_limits<uint32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            } else if (argType->kind == Type::Kind::I32) {
                if (res > std::numeric_limits<int32_t>::max()) {
                    AddError(loc, ErrorCode::ConstOverflow);
                    return std::unexpected(Result::Error);
                }
            }
            return program_->alloc_.Allocate<ast::IntLiteralExpression>(
                loc, argType, res);
        }
    }
    // bool
    if (argType->IsBool()) {
        std::optional<int64_t> rhsVal = TryGetConstInt(rhs);
        DASSERT(rhsVal);

        if (UnaryExpression::IsRelational(op)) {
            const bool res = EvalRelationalOp(op, *rhsVal);
            return program_->alloc_.Allocate<ast::BoolLiteralExpression>(
                loc, returnType, res);
        }
    }
    UNREACHABLE();
    return std::unexpected(Result::Error);
}

Expected<IdentExpression*> ProgramBuilder::CreateIdentExpr(const Ident& ident) {
    // Check name
    ast::Node* symbol = currentScope_->FindSymbol(ident.name);
    if (!symbol) {
        AddError(ident.loc, ErrorCode::SymbolNotFound,
                 "symbol '{}' not found in the current scope", ident.name);
        return std::unexpected(Result::Error);
    }
    if (symbol && !symbol->Is<ast::Variable>()) {
        AddError(ident.loc, ErrorCode::SymbolNotVariable,
                 "symbol '{}' is not a variable", ident.name);
        return std::unexpected(Result::Error);
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
            return std::unexpected(Result::Error);
        }
    } else {
        // AbstractInt || i32
        if (value > std::numeric_limits<int32_t>::max()) {
            AddError(loc, ErrorCode::LiteralInitValueTooLarge);
            return std::unexpected(Result::Error);
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
            return std::unexpected(Result::Error);
        }
    } else {
        AddError(loc, ErrorCode::Unimplemented,
                 "half types are not implemented");
        return std::unexpected(Result::Error);
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
        return std::unexpected(Result::Error);
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
