#pragma once
#include "common.h"
#include "token.h"

#include "base/tree_printer.h"

namespace wgsl::ast {

// Node type and type flags for casting
enum class NodeType : uint64_t {
    Unknown = 0,
    Node = 1 << 0,
    Type = 1 << 1,
    // Expressions
    Expression = 1 << 4,
    UnaryExpression = 1 << 5,
    BinaryExpression = 1 << 6,
    LiteralExpression = 1 << 7,
    FloatLiteralExpression = 1 << 8,
    IntLiteralExpression = 1 << 9,
    BoolLiteralExpression = 1 << 10,
    IdentExpression = 1 << 11,
    // Variables
    Variable = 1 << 15,
    OverrideVariable = 1 << 16,
    ConstVariable = 1 << 17,
    VarVariable = 1 << 18,
    Attribute = 1 << 19,
    // Statements
};
DEFINE_ENUM_FLAGS_OPERATORS(NodeType);

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

private:
    NodeType typeFlags_;
    SourceLoc loc_;
};

}  // namespace wgsl::ast