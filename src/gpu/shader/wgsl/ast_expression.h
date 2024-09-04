#pragma once
#include "ast_node.h"

namespace wgsl::ast { 

class Expression : public Node {
public:
    // TODO: Should we store type?
    // DataType type;

    static NodeType GetStaticType() { return NodeType::Expression; }

protected:
    Expression(LocationRange loc, NodeType type)
        : Node(loc, type | NodeType::Expression) {}
};

class UnaryExpression final : public Expression {
public:
    enum class OpCode {
        BitwiseNot,  // ~
        Address,     // &
        Minus,       // -
        Negation,    // !
    };

    Expression* rhs;
    OpCode op;

public:
    static NodeType GetStaticType() { return NodeType::UnaryExpression; }

    UnaryExpression(LocationRange loc, OpCode op, Expression* rhs)
        : Expression(loc, NodeType::UnaryExpression), rhs(rhs), op(op) {}
};

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
    BinaryExpression(LocationRange loc,
                     Expression* lhs,
                     OpCode op,
                     Expression* rhs)
        : Expression(loc, NodeType::BinaryExpression)
        , lhs(lhs)
        , op(op)
        , rhs(rhs) {}

    static NodeType GetStaticType() { return NodeType::BinaryExpression; }
};

class LiteralExpression : public Expression {
public:
    static NodeType GetStaticType() { return NodeType::LiteralExpression; }

protected:
    LiteralExpression(LocationRange loc, NodeType type)
        : Expression(loc, type | NodeType::LiteralExpression) {}
};

// 3u, 10i
class IntLiteralExpression final : public LiteralExpression {
public:
    enum class Type {
        Abstract,
        I32,
        U32,
    };
    int64_t value;
    Type type;

public:
    IntLiteralExpression(LocationRange loc, int64_t value, Type type)
        : LiteralExpression(loc, NodeType::IntLiteralExpression)
        , value(value)
        , type(type) {}

    static NodeType GetStaticType() { return NodeType::IntLiteralExpression; }
};

// 2.4f, 2.0
class FloatLiteralExpression final : public LiteralExpression {
public:
    enum class Type {
        Abstract,
        F32,
        F16,
    };

    double value;
    Type type;

public:
    FloatLiteralExpression(LocationRange loc, double value, Type type)
        : LiteralExpression(loc, NodeType::FloatLiteralExpression)
        , value(value)
        , type(type) {}

    static NodeType GetStaticType() { return NodeType::FloatLiteralExpression; }
};

class BoolLiteralExpression : public LiteralExpression {
public:
    bool value;

public:
    BoolLiteralExpression(LocationRange loc, bool value)
        : LiteralExpression(loc, NodeType::BoolLiteralExpression)
        , value(value) {}

    static NodeType GetStaticType() { return NodeType::BoolLiteralExpression; }
};

} // namespace wgsl::ast