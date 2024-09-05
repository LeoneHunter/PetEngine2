#pragma once
#include "ast_node.h"

namespace wgsl::ast {

class Expression : public Node {
public:
    constexpr static inline auto kStaticType = NodeType::Expression;

protected:
    Expression(LocationRange loc, NodeType type)
        : Node(loc, type | kStaticType) {}
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

    UnaryExpression(LocationRange loc, OpCode op, Expression* rhs)
        : Expression(loc, kStaticType), rhs(rhs), op(op) {}
};

constexpr std::string to_string(UnaryExpression::OpCode op) {
    using Op = UnaryExpression::OpCode;
    switch(op) {
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

    BinaryExpression(LocationRange loc,
                     Expression* lhs,
                     OpCode op,
                     Expression* rhs)
        : Expression(loc, kStaticType)
        , lhs(lhs)
        , op(op)
        , rhs(rhs) {}
};

constexpr std::string to_string(BinaryExpression::OpCode op) {
    using Op = BinaryExpression::OpCode;
    switch(op) {
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
    LiteralExpression(LocationRange loc, NodeType type)
        : Expression(loc, type | kStaticType) {}
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
    constexpr static inline auto kStaticType = NodeType::IntLiteralExpression;

    IntLiteralExpression(LocationRange loc, int64_t value, Type type)
        : LiteralExpression(loc, kStaticType)
        , value(value)
        , type(type) {}
};

constexpr std::string to_string(IntLiteralExpression::Type type) {
    using Type = IntLiteralExpression::Type;
    switch(type) {
        case Type::Abstract: return "Abstract";
        case Type::I32: return "I32";
        case Type::U32: return "U32";
        default: return "";
    }
}

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
    constexpr static inline auto kStaticType = NodeType::FloatLiteralExpression;

    FloatLiteralExpression(LocationRange loc, double value, Type type)
        : LiteralExpression(loc, kStaticType)
        , value(value)
        , type(type) {}
};

constexpr std::string to_string(FloatLiteralExpression::Type type) {
    using Type = FloatLiteralExpression::Type;
    switch(type) {
        case Type::Abstract: return "Abstract";
        case Type::F32: return "F32";
        case Type::F16: return "F16";
        default: return "";
    }
}

class BoolLiteralExpression : public LiteralExpression {
public:
    bool value;

public:
    constexpr static inline auto kStaticType = NodeType::BoolLiteralExpression;

    BoolLiteralExpression(LocationRange loc, bool value)
        : LiteralExpression(loc, kStaticType)
        , value(value) {}
};

} // namespace wgsl::ast