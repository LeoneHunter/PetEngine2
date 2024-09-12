#pragma once
#include "common.h"
#include "token.h"

#include "base/tree_printer.h"

namespace wgsl::ast {

// Node type and type flags for casting
#define NODE_TYPE_LIST(V)                           \
    V(Node, 1, "node")                              \
    V(Symbol, 2, "symbol")                          \
    /* Types */                                     \
    V(Type, 3, "type")                              \
    V(Scalar, 4, "scalar")                          \
    V(Vector, 5, "vec")                             \
    V(Array, 6, "array")                            \
    V(Matrix, 7, "mat")                             \
    V(Texture, 8, "texture")                        \
    V(Sampler, 9, "sampler")                        \
    V(Alias, 10, "alias")                           \
    V(Struct, 11, "struct")                         \
    V(Member, 12, "member")                         \
    V(Function, 13, "function")                     \
    /* Expressions */                               \
    V(Expression, 20, "expression")                 \
    V(UnaryExpression, 21, "unary expression")      \
    V(BinaryExpression, 22, "binary expression")    \
    V(LiteralExpression, 23, "literal expression")  \
    V(FloatLiteralExpression, 24, "float literal")  \
    V(IntLiteralExpression, 25, "int literal")      \
    V(BoolLiteralExpression, 26, "bool literal")    \
    V(IdentExpression, 27, "identifier expression") \
    /* Variables */                                 \
    V(Variable, 30, "variable")                     \
    V(OverrideVariable, 31, "override variable")    \
    V(ConstVariable, 32, "const value")             \
    V(VarVariable, 33, "var variable")              \
    V(Attribute, 34, "attribute")

enum class NodeType : uint64_t {
#define ENUM(NAME, BIT, STR) NAME = 1ULL << BIT,
    NODE_TYPE_LIST(ENUM)
#undef ENUM
};

constexpr std::string_view to_string(NodeType type) {
#define CASE(NAME, BIT, STR) \
    case NodeType::NAME: return STR;
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