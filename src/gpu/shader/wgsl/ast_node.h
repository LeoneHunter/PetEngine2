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
    /* Expressions */             \
    V(Expression, 20)             \
    V(UnaryExpression, 21)        \
    V(BinaryExpression, 22)       \
    V(LiteralExpression, 23)      \
    V(FloatLiteralExpression, 24) \
    V(IntLiteralExpression, 25)   \
    V(BoolLiteralExpression, 26)  \
    V(IdentExpression, 27)        \
    /* Variables */               \
    V(Variable, 30)               \
    V(OverrideVariable, 31)       \
    V(ConstVariable, 32)          \
    V(VarVariable, 33)            \
    V(Attribute, 34)

enum class NodeType : uint64_t {
#define ENUM(NAME, BIT) NAME = 1ULL << BIT,
    NODE_TYPE_LIST(ENUM)
#undef ENUM
};
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

    Node(NodeType type)
        : typeFlags_(type | kStaticType) {}

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
    Symbol(SourceLoc loc, NodeType type)
        : Node(type | kStaticType) {}

    Symbol(NodeType type)
        : Node(type | kStaticType) {}

};

}  // namespace wgsl::ast