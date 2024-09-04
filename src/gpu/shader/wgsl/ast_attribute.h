#pragma once
#include "ast_expression.h"
#include "ast_node.h"

namespace wgsl::ast {

class Attribute : public Node {
public:
    wgsl::Attribute attr;
    Expression* expr;

public:
    Attribute(LocationRange loc,
              wgsl::Attribute attr,
              Expression* expr = nullptr)
        : Node(loc, NodeType::Attribute), attr(attr), expr(expr) {}

    static NodeType GetStaticType() { return NodeType::Attribute; }
};

}  // namespace wgsl::ast