#pragma once
#include "ast_node.h"
#include "ast_type.h"

namespace wgsl::ast {

// rhs of an assignement or statement
// A unit of computation
class Expression : public Node {
public:
    // Result type of the expression after all conversions
    ast::Type* type = nullptr;

    constexpr static inline auto kStaticType = NodeType::Expression;

protected:
    Expression(SourceLoc loc, NodeType nodeType, Type* type)
        : Node(loc, nodeType | kStaticType), type(type) {}
};

class UnaryExpression final : public Expression {
public:
    enum class OpCode {
        BitwiseNot,  // ~
        Address,     // &
        Minus,       // -
        Negation,    // !
        Deref,       // *
    };

    Expression* rhs;
    OpCode op;

public:
    constexpr static inline auto kStaticType = NodeType::UnaryExpression;

    UnaryExpression(SourceLoc loc, Type* type, OpCode op, Expression* rhs)
        : Expression(loc, kStaticType, type), rhs(rhs), op(op) {}
};

constexpr std::string_view to_string(UnaryExpression::OpCode op) {
    using Op = UnaryExpression::OpCode;
    switch (op) {
        case Op::BitwiseNot: return "BitwiseNot";
        case Op::Address: return "Address";
        case Op::Minus: return "Minus";
        case Op::Negation: return "Negation";
        default: return "";
    }
}

class BinaryExpression final : public Expression {
public:
    enum class OpCode {
        Add,                // +
        Sub,                // -
        Mul,                // *
        Div,                // /
        LessThan,           // <
        GreaterThan,        // >
        LessThanEqual,      // <=
        GreaterThanEqual,   // >=
        Equal,              // ==
        NotEqual,           // !=
        Remainder,          // %
        And,                // &&
        Or,                 // ||
        BitwiseAnd,         // &
        BitwiseOr,          // |
        BitwiseXor,         // ^
        BitwiseLeftShift,   // <<
        BitwiseRightShift,  // >>
    };

    Expression* lhs;
    Expression* rhs;
    OpCode op;

public:
    constexpr static inline auto kStaticType = NodeType::BinaryExpression;

    BinaryExpression(SourceLoc loc,
                     Type* type,
                     Expression* lhs,
                     OpCode op,
                     Expression* rhs)
        : Expression(loc, kStaticType, type), lhs(lhs), op(op), rhs(rhs) {}
};

constexpr std::string_view to_string(BinaryExpression::OpCode op) {
    using Op = BinaryExpression::OpCode;
    switch (op) {
        case Op::Add: return "Add";
        case Op::Sub: return "Sub";
        case Op::Mul: return "Mul";
        case Op::Div: return "Div";
        case Op::LessThan: return "LessThan";
        case Op::GreaterThan: return "GreaterThan";
        case Op::LessThanEqual: return "LessThanEqual";
        case Op::GreaterThanEqual: return "GreaterThanEqual";
        case Op::Equal: return "Equal";
        case Op::NotEqual: return "NotEqual";
        case Op::Remainder: return "Remainder";
        case Op::And: return "And";
        case Op::Or: return "Or";
        case Op::BitwiseAnd: return "BitwiseAnd";
        case Op::BitwiseOr: return "BitwiseOr";
        case Op::BitwiseXor: return "BitwiseXor";
        case Op::BitwiseLeftShift: return "BitwiseLeftShift";
        case Op::BitwiseRightShift: return "BitwiseRightShift";
        default: return "";
    }
}

class LiteralExpression : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::LiteralExpression;

protected:
    LiteralExpression(SourceLoc loc, NodeType nodeType, Type* type)
        : Expression(loc, nodeType | kStaticType, type) {}
};

// An integer value
// 3u, 10i
class IntLiteralExpression final : public LiteralExpression {
public:
    int64_t value;

public:
    constexpr static inline auto kStaticType = NodeType::IntLiteralExpression;

    IntLiteralExpression(SourceLoc loc, Type* type, int64_t value)
        : LiteralExpression(loc, kStaticType, type), value(value) {}
};

// A float value
// 2.4f, 2.0
class FloatLiteralExpression final : public LiteralExpression {
public:
    double value;

public:
    constexpr static inline auto kStaticType = NodeType::FloatLiteralExpression;

    FloatLiteralExpression(SourceLoc loc, Type* type, double value)
        : LiteralExpression(loc, kStaticType, type), value(value) {}
};

// A bool value
class BoolLiteralExpression : public LiteralExpression {
public:
    bool value;

public:
    constexpr static inline auto kStaticType = NodeType::BoolLiteralExpression;

    BoolLiteralExpression(SourceLoc loc, Type* type, bool value)
        : LiteralExpression(loc, kStaticType, type) {}
};

// An identifier
class IdentExpression final : public Expression {
public:
    constexpr static inline auto kStaticType = NodeType::IdentExpression;

    ast::Node* decl;

public:
    IdentExpression(SourceLoc loc, ast::Node* decl, ast::Type* type)
        : Expression(loc, kStaticType, type), decl(decl) {}
};

}  // namespace wgsl::ast