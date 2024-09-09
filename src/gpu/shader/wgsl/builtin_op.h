#pragma once
#include "ast_expression.h"
#include "ast_type.h"
#include "common.h"

namespace wgsl {

using EvalResult = std::variant<double, int64_t, bool>;
using ExpectedType = Expected<ast::Type*>;
using ExpectedEvalResult = Expected<EvalResult>;

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

// Base class for operator helper nodes
// Verifies operator arguments and if both arguments are const, evaluates at comp-time
class BuiltinOp : public ast::Node {
public:
    constexpr static inline auto kStaticType = ast::NodeType::BuiltinOp;

    // For unary operators
    virtual OpResult CheckAndEval(ast::Expression* arg) const = 0;

    // For binary operators
    virtual OpResult CheckAndEval(ast::Expression* lhs,
                                  ast::Expression* rhs) const = 0;

protected:
    BuiltinOp() : ast::Node(kStaticType) {}
};

constexpr std::string_view GetBuiltinOpName(ast::OpCode op) {
    using Op = ast::OpCode;
    switch (op) {
        case Op::Negation: return "__builtin_op_negation";
        case Op::BitNot: return "__builtin_op_bit_not";
        case Op::LogNot: return "__builtin_op_log_not";
        case Op::Add: return "__builtin_op_add";
        case Op::Sub: return "__builtin_op_sub";
        case Op::Mul: return "__builtin_op_mul";
        case Op::Div: return "__builtin_op_div";
        case Op::Mod: return "__builtin_op_mod";
        case Op::LogAnd: return "__builtin_op_log_and";
        case Op::LogOr: return "__builtin_op_log_or";
        case Op::Equal: return "__builtin_op_eq";
        case Op::NotEqual: return "__builtin_op_ne";
        case Op::Less: return "__builtin_op_less";
        case Op::Greater: return "__builtin_op_greater";
        case Op::LessEqual: return "__builtin_op_ge";
        case Op::GreaterEqual: return "__builtin_op_le";
        case Op::BitAnd: return "__builtin_op_bit_and";
        case Op::BitOr: return "__builtin_op_bit_or";
        case Op::BitXor: return "__builtin_op_bit_xor";
        case Op::BitRsh: return "__builtin_op_bit_rsh";
        case Op::BitLsh: return "__builtin_op_bit_lsh";
        default: return "";
    }
}

struct UnaryOp : public BuiltinOp {

    OpResult CheckAndEval(ast::Expression* lhs,
                          ast::Expression* rhs) const override {
        FATAL("Unary operation takes one argument");
    }
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
            if (auto val = arg->TryGetConstValueAs<double>()) {
                res.value = EvalResult(-*val);
            }
        } else if (arg->type->IsInteger()) {
            if (auto val = arg->TryGetConstValueAs<int64_t>()) {
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
        if (auto val = arg->TryGetConstValueAs<bool>()) {
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
        if (auto val = arg->TryGetConstValueAs<int64_t>()) {
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

struct BinaryOp : public BuiltinOp {

    OpResult CheckAndEval(ast::Expression* arg) const override {
        FATAL("Bianry operation takes two arguments");
    }

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
        auto lval = lhs->TryGetConstValueAs<T>();
        auto rval = rhs->TryGetConstValueAs<T>();
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
    ast::OpCode op;

    BinaryArithmeticOp(ast::OpCode op) : op(op) {}

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
    ast::OpCode op;
    ast::Type* boolType;

    BinaryLogicalOp(ast::OpCode op, ast::Type* boolType)
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

// Operators: & | ^ << >>
struct BinaryBitwiseOp : public BinaryOp {
    ast::OpCode op;

    BinaryBitwiseOp(ast::OpCode op) : op(op) {}

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
        if(res.commonType->kind == ast::Type::Kind::U32) {
            Eval<uint64_t, BinaryBitwiseOp>(res, lhs, rhs);
        } else {
            Eval<int64_t, BinaryBitwiseOp>(res, lhs, rhs);
        }
        return res;
    }

    template <class T>
    constexpr int64_t EvalOp(T lhs, T rhs) const {
        using Op = OpCode;
        T val;
        switch (op) {
            case Op::BitAnd: val = lhs & rhs; break;
            case Op::BitOr: val = lhs | rhs; break;
            case Op::BitXor: val = lhs ^ rhs; break;
            case Op::BitLsh: val = lhs << rhs; break;
            case Op::BitRsh: val = lhs >> rhs; break;
            default: return {};
        }
        return static_cast<int64_t>(val);
    }
};

}  // namespace wgsl