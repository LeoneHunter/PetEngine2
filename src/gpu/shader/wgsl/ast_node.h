#pragma once
#include "common.h"
#include "token.h"

#include "base/tree_printer.h"

namespace wgsl::ast {

// Node type and type flags for casting
#define NODE_TYPE_LIST(V)         \
    V(Node, 1)                    \
    V(Symbol, 2)                  \
    /* Types */                   \
    V(Type, 3)                    \
    V(Scalar, 4)                  \
    V(Vector, 5)                  \
    V(Array, 6)                   \
    V(Matrix, 7)                  \
    V(Texture, 8)                 \
    V(Sampler, 9)                 \
    V(Alias, 10)                  \
    V(Struct, 11)                 \
    V(Member, 12)                 \
    V(Function, 13)               \
    V(BuiltinFunction, 14)        \
    /* Expressions */             \
    V(Expression, 15)             \
    V(UnaryExpression, 16)        \
    V(BinaryExpression, 17)       \
    V(LiteralExpression, 18)      \
    V(FloatLiteralExpression, 19) \
    V(IntLiteralExpression, 20)   \
    V(BoolLiteralExpression, 21)  \
    V(IdentExpression, 22)        \
    V(MemberAccessExpr, 23)       \
    V(SwizzleExpr, 24)            \
    V(ArrayIndexExpr, 25)         \
    /* Variables */               \
    V(Variable, 30)               \
    V(OverrideVariable, 31)       \
    V(ConstVariable, 32)          \
    V(VarVariable, 33)            \
    V(Parameter, 34)              \
    /* Attributes */              \
    V(Attribute, 35)              \
    V(ScalarAttribute, 36)        \
    V(BuiltinAttribute, 37)       \
    V(WorkgroupAttribute, 38)     \
    /* Statement */               \
    V(Statement, 39)              \
    V(CompoundStatement, 40)      \
    V(AssignStatement, 41)        \
    V(ReturnStatement, 43)        \
    V(CallStatement, 44)          \
    V(IfStatement, 45)            \
    V(SwitchStatement, 46)        \
    V(LoopStatement, 47)          \
    V(ForStatement, 48)           \
    V(WhileStatement, 49)         \
    V(BreakStatement, 50)         \
    V(ContinueStatement, 51)      \
    V(DiscardStatement, 52)       \
    V(ScopedStatement, 53)        \
    /* Scope */                   \
    V(GlobalScope, 63)

enum class NodeType : uint64_t {
#define ENUM(NAME, BIT) NAME = 1ULL << BIT,
    NODE_TYPE_LIST(ENUM)
#undef ENUM
};

constexpr std::string_view to_string(NodeType type) {
#define CASE(NAME, BIT) \
    case NodeType::NAME: return #NAME;
    switch (type) {
        NODE_TYPE_LIST(CASE)
        default: return "";
    }
#undef CASE
}

DEFINE_ENUM_FLAGS_OPERATORS(NodeType);

// A base class for various classes owned by Program
// Main subclasses are ast nodes
class Node {
public:
    template <std::derived_from<Node> T>
    bool Is() const {
        return typeFlags_ & T::kStaticType;
    }

    template <std::derived_from<Node> T>
    T* As() {
        return Is<T>() ? static_cast<T*>(this) : nullptr;
    }

    template <std::derived_from<Node> T>
    const T* As() const {
        return Is<T>() ? static_cast<const T*>(this) : nullptr;
    }

    // node source range, including end: [start, end]
    SourceLoc GetLoc() const { return loc_; }

public:
    constexpr static inline auto kStaticType = NodeType::Node;

    virtual ~Node() = default;

protected:
    Node(SourceLoc loc, NodeType type)
        : typeFlags_(type | kStaticType), loc_(loc) {}

    Node(NodeType type) : typeFlags_(type | kStaticType) {}

private:
    NodeType typeFlags_;
    SourceLoc loc_;
};

// A node with a unique program scope name
// - A type
// - A variable
class Symbol : public Node {
public:
    static constexpr auto kStaticType = NodeType::Symbol;

protected:
    Symbol(SourceLoc loc, NodeType type) : Node(type | kStaticType) {}

    Symbol(NodeType type) : Node(type | kStaticType) {}
};

}  // namespace wgsl::ast