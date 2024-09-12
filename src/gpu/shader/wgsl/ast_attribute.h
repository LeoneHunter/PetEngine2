#pragma once
#include "ast_node.h"
#include "program_alloc.h"

namespace wgsl::ast {

class Expression;
class Attribute;
using AttributeList = ProgramVector<const Attribute*>;

// Base class for attributes and 
// valueless attributes: @must_use, @compute, @vertex, @fragment
class Attribute : public Node {
public:
    const AttributeName attr;

    constexpr static inline auto kStaticType = NodeType::Attribute;

    Attribute(SourceLoc loc, AttributeName attr)
        : Node(loc, kStaticType), attr(attr) {}

protected:
    Attribute(SourceLoc loc, NodeType nodeType, AttributeName attr)
        : Node(loc, nodeType | kStaticType), attr(attr) {}
};

// For single numeric value attributes:
//   @align, @location, @size, @binding, @group, @id
class ScalarAttribute final : public Attribute {
public:
    const int64_t value;

    constexpr static inline auto kStaticType = NodeType::ScalarAttribute;

    ScalarAttribute(SourceLoc loc, AttributeName attr, int64_t value)
        : Attribute(loc, kStaticType, attr), value(value) {}
};

// For builtin values
class BuiltinAttribute final : public Attribute {
public:
    const Builtin value;

    constexpr static inline auto kStaticType = NodeType::BuiltinAttribute;

    BuiltinAttribute(SourceLoc loc, Builtin value)
        : Attribute(loc, kStaticType, AttributeName::Builtin), value(value) {}
};

// For compute shader workgroup settings
class WorkgroupAttribute final : public Attribute {
public:
    const uint32_t x;
    const uint32_t y;
    const uint32_t z;

    constexpr static inline auto kStaticType = NodeType::WorkgroupAttribute;

    WorkgroupAttribute(SourceLoc loc, int64_t x, int64_t y, int64_t z)
        : Attribute(loc, kStaticType, AttributeName::Builtin)
        , x(x)
        , y(y)
        , z(z) {}
};

}  // namespace wgsl::ast