#pragma once
#include "common.h"

namespace wgsl::ast {

// Node type and type flags for casting
enum class NodeType: uint64_t {
    Unknown = 0,
    Node = 0x1,
    // Expressions
    Expression = 0x2,
    UnaryExpression = 0x4,
    BinaryExpression = 0x8,
    LiteralExpression = 0x10,
    FloatLiteralExpression = 0x20,
    IntLiteralExpression = 0x40,
    BoolLiteralExpression = 0x80,
    // Variables
    Variable = 0x100,
    OverrideVariable = 0x200,
    ConstVariable = 0x400,
    VarVariable = 0x800,
    Attribute = 0x1000,
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
    LocationRange GetLocRange() const { return loc_; }

    Location GetLocStart() const { return loc_.start; }
    Location GetLocEnd() const { return loc_.end; }

public:
    constexpr static inline auto kStaticType = NodeType::Node;

    virtual ~Node() = default;

protected:
    Node(LocationRange loc, NodeType type)
        : typeFlags_(type | kStaticType), loc_(loc) {}

private:
    NodeType typeFlags_;
    LocationRange loc_;
};

} // namespace wgsl