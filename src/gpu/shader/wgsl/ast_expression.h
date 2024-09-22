#pragma once
#include "ast_function.h"
#include "ast_node.h"
#include "ast_type.h"
#include "ast_variable.h"

namespace wgsl::ast {

class Function;
class Struct;
class Member;

// Builtin operators
#define OP_CODES(V)       \
    /* UNARY */           \
    V(Negation, "-")      \
    V(BitNot, "~")        \
    V(LogNot, "!")        \
    /* BINARY */          \
    /* arithmetic */      \
    V(Add, "+")           \
    V(Sub, "-")           \
    V(Mul, "*")           \
    V(Div, "/")           \
    V(Mod, "%")           \
    /* logical */         \
    V(LogAnd, "&&")       \
    V(LogOr, "||")        \
    V(Equal, "==")        \
    V(NotEqual, "!=")     \
    V(Less, "<")          \
    V(Greater, ">")       \
    V(GreaterEqual, ">=") \
    V(LessEqual, "<=")    \
    /* bitwise */         \
    V(BitAnd, "&")        \
    V(BitOr, "|")         \
    V(BitXor, "^")        \
    V(BitLsh, "<<")       \
    V(BitRsh, ">>")

// Define enum
#define ENUM(Name, Str) Name,
enum class OpCode : uint8_t {
    OP_CODES(ENUM) _Max,
};
#undef ENUM

constexpr std::string_view to_string(OpCode code) {
#define CASE(Name, Str) \
    case OpCode::Name: return Str;
    switch (code) {
        OP_CODES(CASE)
        default: return "";
    }
#undef CASE
}

constexpr bool IsOpUnary(OpCode op) {
    return op <= OpCode::LogNot;
}

constexpr bool IsOpBinary(OpCode op) {
    return !IsOpUnary(op);
}

constexpr bool IsOpArithmetic(OpCode op) {
    switch (op) {
        case OpCode::Negation:
        case OpCode::Add:
        case OpCode::Sub:
        case OpCode::Mul:
        case OpCode::Div:
        case OpCode::Mod: return true;
        default: return false;
    }
}

constexpr bool IsOpLogical(OpCode op) {
    switch (op) {
        case OpCode::LogNot:
        case OpCode::LogAnd:
        case OpCode::LogOr:
        case OpCode::Equal:
        case OpCode::NotEqual:
        case OpCode::Less:
        case OpCode::Greater:
        case OpCode::LessEqual:
        case OpCode::GreaterEqual: return true;
        default: return false;
    }
}

constexpr bool IsOpBitwise(OpCode op) {
    switch (op) {
        case OpCode::BitNot:
        case OpCode::BitAnd:
        case OpCode::BitOr:
        case OpCode::BitXor:
        case OpCode::BitLsh:
        case OpCode::BitRsh: return true;
        default: return false;
    }
}

// rhs of an assignement or statement
// A unit of computation
class Expression : public Node {
public:
    constexpr static inline auto kStaticType = NodeType::Expression;

    // Result type of the expression after all conversions
    const ast::Type* type = nullptr;

    template <class T>
    std::optional<T> TryGetConstValueAs() const;

    Expression(SourceLoc loc, NodeType nodeType, const Type* type)
        : Node(loc, nodeType | kStaticType), type(type) {}
};

// Unary expression: !, ~, -
class UnaryExpression final : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::UnaryExpression;

    const Expression* rhs;
    OpCode op;

    UnaryExpression(SourceLoc loc,
                    const Type* type,
                    OpCode op,
                    const Expression* rhs)
        : Expression(loc, kStaticType, type), rhs(rhs), op(op) {}

    constexpr bool IsArithmetic() const { return IsOpArithmetic(op); }
    constexpr bool IsLogical() { return IsOpLogical(op); }
    constexpr bool IsBitwise() { return IsOpBitwise(op); }
};

// Binary expression: * / - = > < etc.
class BinaryExpression final : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::BinaryExpression;

    const Expression* lhs;
    const Expression* rhs;
    OpCode op;

    BinaryExpression(SourceLoc loc,
                     const Type* type,
                     const Expression* lhs,
                     OpCode op,
                     const Expression* rhs)
        : Expression(loc, kStaticType, type), lhs(lhs), op(op), rhs(rhs) {}

    constexpr bool IsArithmetic() const { return IsOpArithmetic(op); }
    constexpr bool IsLogical() { return IsOpLogical(op); }
    constexpr bool IsBitwise() { return IsOpBitwise(op); }
};


class LiteralExpression : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::LiteralExpression;

    LiteralExpression(SourceLoc loc, NodeType nodeType, const Type* type)
        : Expression(loc, nodeType | kStaticType, type) {}
};

// An integer value
// 3u, 10i
class IntLiteralExpression final : public LiteralExpression {
public:
    int64_t value;

    constexpr static inline auto kStaticType = NodeType::IntLiteralExpression;

    IntLiteralExpression(SourceLoc loc, const Type* type, int64_t value)
        : LiteralExpression(loc, kStaticType, type), value(value) {}
};

// A float value
// 2.4f, 2.0
class FloatLiteralExpression final : public LiteralExpression {
public:
    double value;

    constexpr static inline auto kStaticType = NodeType::FloatLiteralExpression;

    FloatLiteralExpression(SourceLoc loc, const Type* type, double value)
        : LiteralExpression(loc, kStaticType, type), value(value) {}
};

// A bool value
class BoolLiteralExpression : public LiteralExpression {
public:
    bool value;

    constexpr static inline auto kStaticType = NodeType::BoolLiteralExpression;

    BoolLiteralExpression(SourceLoc loc, const Type* type, bool value)
        : LiteralExpression(loc, kStaticType, type), value(value) {}
};

// An identifier
class IdentExpression final : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::IdentExpression;
    // Variable Type or Function
    const ast::Symbol* symbol;

    IdentExpression(SourceLoc loc, const ast::Variable* var)
        : Expression(loc, kStaticType, var->type), symbol(var) {}

    IdentExpression(SourceLoc loc, const ast::Type* type)
        : Expression(loc, kStaticType, type), symbol(type) {}

    IdentExpression(SourceLoc loc, const ast::Function* func)
        : Expression(loc, kStaticType, nullptr), symbol(func) {}
};



// Struct access expression: struct.member
class MemberAccessExpr final : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::MemberAccessExpr;
    const Expression* expr;
    const Member* member;

    MemberAccessExpr(SourceLoc loc,
                     const Expression* expr,
                     const Member* member)
        : Expression(loc, kStaticType, member->type)
        , expr(expr)
        , member(member) {}
};



// Vector components swizzle op expr; a.x, a.xyz, a.rgba
class SwizzleExpr final : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::SwizzleExpr;

    const Expression* lhs;
    const std::array<VecComponent, 4> swizzle;

    SwizzleExpr(SourceLoc loc,
                const Type* type,
                const Expression* lhs,
                VecComponent x,
                VecComponent y = VecComponent::None,
                VecComponent z = VecComponent::None,
                VecComponent w = VecComponent::None)
        : Expression(loc, kStaticType, type), lhs(lhs), swizzle{x, y, z, w} {}

    SwizzleExpr(SourceLoc loc,
                const Type* type,
                const Expression* lhs,
                std::span<VecComponent> span)
        : Expression(loc, kStaticType, type), lhs(lhs), swizzle{[&] {
            std::array<VecComponent, 4> buf{};
            for (uint32_t i = 0; i < span.size() && i < 4; ++i) {
                buf[i] = span[i];
            }
            return buf;
        }()} {}
};



// Array accessed by index
class ArrayIndexExpr final : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::ArrayIndexExpr;
    const Expression* array;
    const Expression* indexExpr;

    ArrayIndexExpr(SourceLoc loc,
                   const Type* type,
                   const Expression* arr,
                   const Expression* indexExpr)
        : Expression(loc, kStaticType, type)
        , array(arr)
        , indexExpr(indexExpr) {}
};

}  // namespace wgsl::ast

template <class T>
std::optional<T> wgsl::ast::Expression::TryGetConstValueAs() const {
    if (auto e = this->As<FloatLiteralExpression>()) {
        return static_cast<T>(e->value);
    }
    if (auto e = this->As<IntLiteralExpression>()) {
        return static_cast<T>(e->value);
    }
    if (auto e = this->As<BoolLiteralExpression>()) {
        return static_cast<T>(e->value);
    }
    // Ident
    if (auto ident = this->As<IdentExpression>()) {
        if (auto var = ident->symbol->As<ConstVariable>()) {
            return var->initializer->TryGetConstValueAs<T>();
        }
    }
    return std::nullopt;
}