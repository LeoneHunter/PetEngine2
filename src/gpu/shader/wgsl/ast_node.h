#pragma once
#include "common.h"
#include "token.h"

#include "base/tree_printer.h"

namespace wgsl::ast {

// Node type and type flags for casting
enum class NodeType : uint64_t {
    Unknown = 0,
    Node = 1 << 0,
    // Support nodes
    Type = 1 << 1,
    BuiltinOp = 1 << 2,
    TypeGen = 1 << 3,
    // Expressions
    Expression = 1 << 10,
    UnaryExpression = 1 << 11,
    BinaryExpression = 1 << 12,
    LiteralExpression = 1 << 13,
    FloatLiteralExpression = 1 << 14,
    IntLiteralExpression = 1 << 15,
    BoolLiteralExpression = 1 << 16,
    IdentExpression = 1 << 17,
    // Variables
    Variable = 1 << 20,
    OverrideVariable = 1 << 21,
    ConstVariable = 1 << 22,
    VarVariable = 1 << 23,
    Attribute = 1 << 24,
    // Statements
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

}  // namespace wgsl::ast