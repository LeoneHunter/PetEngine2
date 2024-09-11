#pragma once
#include "ast_node.h"
#include "ast_type.h"
#include "ast_variable.h"
#include "ast_function.h"

namespace wgsl::ast {

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

class Function;

// rhs of an assignement or statement
// A unit of computation
class Expression : public Node {
public:
    constexpr static inline auto kStaticType = NodeType::Expression;

    // Result type of the expression after all conversions
    const ast::Type* type = nullptr;

public:
    template <class T>
    std::optional<T> TryGetConstValueAs() const;

protected:
    Expression(SourceLoc loc, NodeType nodeType, const Type* type)
        : Node(loc, nodeType | kStaticType), type(type) {}
};

class UnaryExpression final : public Expression {
public:
    const Expression* rhs;
    OpCode op;

public:
    constexpr static inline auto kStaticType = NodeType::UnaryExpression;

    UnaryExpression(SourceLoc loc,
                    const Type* type,
                    OpCode op,
                    const Expression* rhs)
        : Expression(loc, kStaticType, type), rhs(rhs), op(op) {}

    constexpr bool IsArithmetic() const { return IsOpArithmetic(op); }
    constexpr bool IsLogical() { return IsOpLogical(op); }
    constexpr bool IsBitwise() { return IsOpBitwise(op); }
};

class BinaryExpression final : public Expression {
public:
    const Expression* lhs;
    const Expression* rhs;
    OpCode op;

public:
    constexpr static inline auto kStaticType = NodeType::BinaryExpression;

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

protected:
    LiteralExpression(SourceLoc loc, NodeType nodeType, const Type* type)
        : Expression(loc, nodeType | kStaticType, type) {}
};

// An integer value
// 3u, 10i
class IntLiteralExpression final : public LiteralExpression {
public:
    int64_t value;

public:
    constexpr static inline auto kStaticType = NodeType::IntLiteralExpression;

    IntLiteralExpression(SourceLoc loc, const Type* type, int64_t value)
        : LiteralExpression(loc, kStaticType, type), value(value) {}
};

// A float value
// 2.4f, 2.0
class FloatLiteralExpression final : public LiteralExpression {
public:
    double value;

public:
    constexpr static inline auto kStaticType = NodeType::FloatLiteralExpression;

    FloatLiteralExpression(SourceLoc loc, const Type* type, double value)
        : LiteralExpression(loc, kStaticType, type), value(value) {}
};

// A bool value
class BoolLiteralExpression : public LiteralExpression {
public:
    bool value;

public:
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

public:
    IdentExpression(SourceLoc loc, const ast::Variable* var)
        : Expression(loc, kStaticType, var->type), symbol(var) {}

    IdentExpression(SourceLoc loc, const ast::Type* type)
        : Expression(loc, kStaticType, type), symbol(type) {}

    IdentExpression(SourceLoc loc, const ast::Function* func)
        : Expression(loc, kStaticType, nullptr), symbol(func) {}
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