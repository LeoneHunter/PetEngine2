#pragma once
#include "ast_node.h"
#include "program_alloc.h"

namespace wgsl::ast {

class Expression;
class Attribute;
using AttributeList = ProgramVector<const Attribute*>;

class Attribute : public Node {
public:
    AttributeName attr;
    const Expression* expr;

public:
    constexpr static inline auto kStaticType = NodeType::Attribute;

    Attribute(SourceLoc loc,
              AttributeName attr,
              const Expression* expr = nullptr)
        : Node(loc, kStaticType), attr(attr), expr(expr) {}
};

}  // namespace wgsl::ast