#include "program_builder.h"
#include "parser.h"
#include "program.h"

namespace wgsl {

namespace {

using EvalResult = std::variant<double, int64_t, bool>;
using ExpectedType = Expected<ast::Type*>;
using ExpectedEvalResult = Expected<EvalResult>;

template <class T>
std::optional<T> TryGetConstValueAs(ast::Expression* expr) {
    if (!expr) {
        return std::nullopt;
    }
    if (auto e = expr->As<FloatLiteralExpression>()) {
        return static_cast<T>(e->value);
    }
    if (auto e = expr->As<IntLiteralExpression>()) {
        return static_cast<T>(e->value);
    }
    if (auto e = expr->As<BoolLiteralExpression>()) {
        return static_cast<T>(e->value);
    }
    // Ident
    if (auto ident = expr->As<IdentExpression>()) {
        if (auto var = ident->decl->As<ConstVariable>()) {
            return TryGetConstValueAs<T>(var->initializer);
        }
    }
    return std::nullopt;
}

constexpr bool CheckOverflow(ast::Type* type, bool val) {
    return true;
}

constexpr bool CheckOverflow(ast::Type* type, int64_t val) {
    if (type->kind == Type::Kind::U32) {
        return val <= std::numeric_limits<uint32_t>::max();
    }
    if (type->kind == Type::Kind::I32) {
        return val <= std::numeric_limits<int32_t>::max();
    }
    return true;
}

constexpr bool CheckOverflow(ast::Type* type, double val) {
    if (type->kind == Type::Kind::F32) {
        return val <= std::numeric_limits<float>::max();
    }
    // TODO: half not implemented
    return true;
}

// Type analysis result of an operator expression
// E.g. (3 + 3.0f) -> { common_type: f32, return_type: f32, value: 6.0f }
// E.g. (true && true) -> { common_type: bool, return_type: bool, value: true }
struct OpResult {
    // Common type for binary operators
    // null if no conversion possible
    //   u32, f32 -> error
    //   f32, abstr_int -> f32
    //   abstr_int, abstr_float -> abst_float
    ast::Type* commonType = nullptr;
    // Result type of the operation
    // For arithmetic -> common type
    // For logical -> bool
    ast::Type* returnType = nullptr;
    // If all arguments are const tries to evaluate
    std::optional<EvalResult> value;
    // Result error code
    // InvalidArgs if input types are not matched. I.e. float for boolean op
    // ConstOverflow if types are valid and const but operation overflows
    ErrorCode err = ErrorCode::Ok;
};

struct UnaryOp {
    virtual OpResult CheckAndEval(ast::Expression* arg) const = 0;
};

// Only negation: '-'
struct UnaryArithmeticOp : public UnaryOp {
    OpResult CheckAndEval(ast::Expression* arg) const override {
        DASSERT(arg);
        DASSERT(arg->type);
        OpResult res;
        res.commonType = arg->type;
        res.returnType = arg->type;
        // Check type
        if (!arg->type->IsArithmetic()) {
            res.err = ErrorCode::InvalidArgs;
            return res;
        }
        // Try evaluate if const
        if (arg->type->IsFloat()) {
            if (auto val = TryGetConstValueAs<double>(arg)) {
                res.value = EvalResult(-*val);
            }
        } else if (arg->type->IsInteger()) {
            if (auto val = TryGetConstValueAs<int64_t>(arg)) {
                res.value = EvalResult(-*val);
            }
        }
        return res;
    }
};

// Only logical negation: '!'
struct UnaryLogicalOp : public UnaryOp {
    ast::Type* boolType = nullptr;

    UnaryLogicalOp(ast::Type* boolType) : boolType(boolType) {}

    OpResult CheckAndEval(ast::Expression* arg) const override {
        DASSERT(arg);
        DASSERT(arg->type);
        OpResult res;
        res.commonType = arg->type;
        res.returnType = boolType;
        // Check type
        if (!arg->type->IsBool()) {
            res.err = ErrorCode::InvalidArgs;
            return res;
        }
        // Try evaluate if const
        if (auto val = TryGetConstValueAs<bool>(arg)) {
            res.value = EvalResult(!*val);
        }
        return res;
    }
};

// Only bitwise negation: '~'
struct UnaryBitwiseOp : public UnaryOp {
    OpResult CheckAndEval(ast::Expression* arg) const override {
        DASSERT(arg);
        DASSERT(arg->type);
        OpResult res;
        res.commonType = arg->type;
        res.returnType = arg->type;
        // Check type
        if (!arg->type->IsInteger()) {
            res.err = ErrorCode::InvalidArgs;
            return res;
        }
        // Try evaluate if const
        if (auto val = TryGetConstValueAs<int64_t>(arg)) {
            int64_t resultVal = 0;
            if (arg->type->IsSigned()) {
                resultVal = ~*val;
            } else {
                if (!CheckOverflow(res.returnType, *val)) {
                    res.err = ErrorCode::ConstOverflow;
                    return res;
                }
                const auto val32 = ~(static_cast<uint32_t>(*val));
                resultVal = static_cast<int64_t>(val32);
            }
            res.value = EvalResult(resultVal);
        }
        return res;
    }
};

struct BinaryOp {
    virtual OpResult CheckAndEval(ast::Expression* lhs,
                                  ast::Expression* rhs) const = 0;

    // Checks if two types can be converted to a common type
    Expected<ast::Type*> GetCommonType(ast::Expression* lhs,
                                       ast::Expression* rhs) const {
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
            return std::unexpected(ErrorCode::InvalidArgs);
        }
        // Find common type
        auto* resultType = lhs->type;
        if (rl > lr) {
            resultType = rhs->type;
        }
        return resultType;
    }

    bool CheckTypes(OpResult& out,
                    ast::Expression* lhs,
                    ast::Expression* rhs) const {
        DASSERT(lhs && rhs);
        DASSERT(lhs->type && rhs->type);
        // Check types
        if (auto result = GetCommonType(lhs, rhs)) {
            out.commonType = *result;
        } else {
            out.err = ErrorCode::InvalidArgs;
            return false;
        }
        return true;
    }

    // Evaluates an expression using 'EvalOp()' from 'Derived'
    template <class T, class Derived>
    void Eval(OpResult& res, ast::Expression* lhs, ast::Expression* rhs) const {
        auto lval = TryGetConstValueAs<T>(lhs);
        auto rval = TryGetConstValueAs<T>(rhs);
        auto* retType = res.returnType;

        if (lval && rval) {
            auto* derived = (Derived*)this;
            const auto result = derived->EvalOp<T>(*lval, *rval);

            if (!retType->IsAbstract() && !CheckOverflow(retType, result)) {
                res.err = ErrorCode::ConstOverflow;
            }
            res.value = result;
        }
    }
};

// Operators: + - * / %
struct BinaryArithmeticOp : public BinaryOp {
    OpCode op;

    BinaryArithmeticOp(OpCode op) : op(op) {}

    OpResult CheckAndEval(ast::Expression* lhs,
                          ast::Expression* rhs) const override {
        OpResult res;
        if (!CheckTypes(res, lhs, rhs)) {
            return res;
        }
        res.returnType = res.commonType;
        if (!res.returnType->IsArithmetic()) {
            res.err = ErrorCode::InvalidArgs;
            return res;
        }
        ast::Type* commonType = res.commonType;
        // Try evaluate if const
        if (commonType->IsFloat()) {
            Eval<double, BinaryArithmeticOp>(res, lhs, rhs);
        } else if (commonType->IsInteger()) {
            Eval<int64_t, BinaryArithmeticOp>(res, lhs, rhs);
        }
        return res;
    }

    // Used by 'Eval' from parent
    template <class T>
    constexpr T EvalOp(T lhs, T rhs) const {
        using Op = OpCode;
        switch (op) {
            case Op::Add: return lhs + rhs;
            case Op::Sub: return lhs - rhs;
            case Op::Mul: return lhs * rhs;
            case Op::Div: return lhs / rhs;
            default: return {};
        }
    }
};

// Operators: > >= < <= == !=
struct BinaryLogicalOp : public BinaryOp {
    OpCode op;
    ast::Type* boolType;

    BinaryLogicalOp(OpCode op, ast::Type* boolType)
        : op(op), boolType(boolType) {}

    OpResult CheckAndEval(ast::Expression* lhs,
                          ast::Expression* rhs) const override {
        OpResult res;
        if (!CheckTypes(res, lhs, rhs)) {
            return res;
        }
        ast::Type* commonType = res.commonType;
        res.returnType = boolType;

        // Try evaluate if const
        if (commonType->IsFloat()) {
            Eval<double, BinaryLogicalOp>(res, lhs, rhs);
        } else if (commonType->IsInteger()) {
            Eval<int64_t, BinaryLogicalOp>(res, lhs, rhs);
        } else if (commonType->IsBool()) {
            Eval<bool, BinaryLogicalOp>(res, lhs, rhs);
        }
        return res;
    }

    template <class T>
    constexpr bool EvalOp(T lhs, T rhs) const {
        using Op = OpCode;
        switch (op) {
            case Op::Less: return lhs < rhs;
            case Op::Greater: return lhs > rhs;
            case Op::LessEqual: return lhs <= rhs;
            case Op::GreaterEqual: return lhs >= rhs;
            case Op::Equal: return lhs == rhs;
            case Op::NotEqual: return lhs != rhs;
            case Op::LogAnd: return lhs && rhs;
            case Op::LogOr: return lhs || rhs;
            default: return {};
        }
    }
};

// Operators:
struct BinaryBitwiseOp : public BinaryOp {
    OpCode op;

    BinaryBitwiseOp(OpCode op) : op(op) {}

    OpResult CheckAndEval(ast::Expression* lhs,
                          ast::Expression* rhs) const override {
        OpResult res;
        if (!CheckTypes(res, lhs, rhs)) {
            return res;
        }
        res.returnType = res.commonType;
        if (!res.returnType->IsInteger()) {
            res.err = ErrorCode::InvalidArgs;
            return res;
        }
        Eval<int64_t, BinaryBitwiseOp>(res, lhs, rhs);
        return res;
    }

    template <class T>
    constexpr bool EvalOp(T lhs, T rhs) const {
        using Op = OpCode;
        switch (op) {
            case Op::Less: return lhs < rhs;
            case Op::Greater: return lhs > rhs;
            case Op::LessEqual: return lhs <= rhs;
            case Op::GreaterEqual: return lhs >= rhs;
            case Op::Equal: return lhs == rhs;
            case Op::NotEqual: return lhs != rhs;
            case Op::LogAnd: return lhs && rhs;
            case Op::LogOr: return lhs || rhs;
            default: return {};
        }
    }
};

}  // namespace

// Helper to check and evaluate builtin operations
struct ProgramBuilder::OpTable {
    OpTable(ast::Type* boolType) {
        unaryMap.resize(uint8_t(OpCode::_Max));
        unaryMap[uint8_t(OpCode::Negation)] = new UnaryArithmeticOp();
        unaryMap[uint8_t(OpCode::BitNot)] = new UnaryBitwiseOp();
        unaryMap[uint8_t(OpCode::LogNot)] = new UnaryLogicalOp(boolType);

        binaryOp.resize(uint8_t(OpCode::_Max));
        binaryOp[uint8_t(OpCode::Mul)] = new BinaryArithmeticOp(OpCode::Mul);
        binaryOp[uint8_t(OpCode::Div)] = new BinaryArithmeticOp(OpCode::Div);
        binaryOp[uint8_t(OpCode::Add)] = new BinaryArithmeticOp(OpCode::Add);
        binaryOp[uint8_t(OpCode::Sub)] = new BinaryArithmeticOp(OpCode::Sub);
        binaryOp[uint8_t(OpCode::Mod)] = new BinaryArithmeticOp(OpCode::Mod);

        binaryOp[uint8_t(OpCode::LogAnd)] =
            new BinaryLogicalOp(OpCode::LogAnd, boolType);
        binaryOp[uint8_t(OpCode::LogOr)] =
            new BinaryLogicalOp(OpCode::LogOr, boolType);
        binaryOp[uint8_t(OpCode::Less)] =
            new BinaryLogicalOp(OpCode::Less, boolType);
        binaryOp[uint8_t(OpCode::Greater)] =
            new BinaryLogicalOp(OpCode::Greater, boolType);
        binaryOp[uint8_t(OpCode::LessEqual)] =
            new BinaryLogicalOp(OpCode::LessEqual, boolType);
        binaryOp[uint8_t(OpCode::GreaterEqual)] =
            new BinaryLogicalOp(OpCode::GreaterEqual, boolType);
        binaryOp[uint8_t(OpCode::Equal)] =
            new BinaryLogicalOp(OpCode::Equal, boolType);
        binaryOp[uint8_t(OpCode::NotEqual)] =
            new BinaryLogicalOp(OpCode::NotEqual, boolType);

        binaryOp[uint8_t(OpCode::BitAnd)] = new BinaryBitwiseOp(OpCode::BitAnd);
        binaryOp[uint8_t(OpCode::BitOr)] = new BinaryBitwiseOp(OpCode::BitOr);
        binaryOp[uint8_t(OpCode::BitXor)] = new BinaryBitwiseOp(OpCode::BitXor);
        binaryOp[uint8_t(OpCode::BitLsh)] = new BinaryBitwiseOp(OpCode::BitLsh);
        binaryOp[uint8_t(OpCode::BitRsh)] = new BinaryBitwiseOp(OpCode::BitRsh);
    }

    UnaryOp* GetUnaryOp(OpCode op) {
        if (!IsOpUnary(op)) {
            return nullptr;
        }
        return unaryMap[uint8_t(op)];
    }

    BinaryOp* GetBinaryOp(OpCode op) {
        if (IsOpUnary(op)) {
            return nullptr;
        }
        return binaryOp[uint8_t(op)];
    }

    std::vector<UnaryOp*> unaryMap;
    std::vector<BinaryOp*> binaryOp;
};

//===========================================================================//

ProgramBuilder::ProgramBuilder() : program_(new Program()) {}

ProgramBuilder::~ProgramBuilder() {}

void ProgramBuilder::Build(std::string_view code) {
    // Copy source code
    program_->sourceCode_ = std::string(code);
    code = program_->sourceCode_;
    currentScope_ = program_->globalScope_;

    program_->CreateBuiltinTypes();
    ast::Type* type = currentScope_->FindType("bool");
    DASSERT(type);
    opTable_.reset(new OpTable(type));

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
    BinaryOp* handler = opTable_->GetBinaryOp(op);
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
    // TODO: Handle templates
    UnaryOp* handler = opTable_->GetUnaryOp(op);
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
