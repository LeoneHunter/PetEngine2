#pragma once
#include "ast_expression.h"
#include "ast_node.h"

namespace wgsl::ast {

class Attribute : public Node {
public:
    wgsl::AttributeName attr;
    Expression* expr;

public:
    constexpr static inline auto kStaticType = NodeType::Attribute;

    Attribute(SourceLoc loc,
              wgsl::AttributeName attr,
              Expression* expr = nullptr)
        : Node(loc, kStaticType), attr(attr), expr(expr) {}
};

}  // namespace wgsl::ast