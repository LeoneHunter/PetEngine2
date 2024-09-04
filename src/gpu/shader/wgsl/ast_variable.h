#pragma once
#include "ast_node.h"
#include "ast_expression.h"

namespace wgsl::ast {

// Abstract base for 'var', 'override', 'const'
class Variable : public Node {
public:
    // TODO: Should we include type here?
    DataType type;
    std::string ident;
    std::vector<Attribute> attributes;
    ast::Expression* initializer;

    static NodeType GetStaticType() { return NodeType::Variable; }

protected:
    Variable(LocationRange loc,
             NodeType type,
             const std::string& ident,
             const std::vector<Attribute>& attributes,
             Expression* initializer)
        : Node(loc, type | NodeType::Variable)
        , ident(ident)
        , attributes(attributes)
        , initializer(initializer) {}
};

// 'var' 
class VarVariable final : public Variable {
public:
    VarVariable(LocationRange loc,
                const std::string& ident,
                const std::vector<Attribute>& attributes,
                Expression* initializer)
        : Variable(loc, NodeType::VarVariable, ident, attributes, initializer) {
    }

    static NodeType GetStaticType() { return NodeType::VarVariable; }
};

// 'const' 
class ConstVariable final : public Variable {
public:
    ConstVariable(LocationRange loc,
                  const std::string& ident,
                  Expression* initializer)
        : Variable(loc, NodeType::ConstVariable, ident, {}, initializer) {}

    static NodeType GetStaticType() { return NodeType::ConstVariable; }
};

// 'override' 
class OverrideVariable final : public Variable {
public:
    OverrideVariable(LocationRange loc,
                     const std::string& ident,
                     const std::vector<Attribute>& attributes,
                     Expression* initializer)
        : Variable(loc,
                   NodeType::OverrideVariable,
                   ident,
                   attributes,
                   initializer) {}

    static NodeType GetStaticType() { return NodeType::OverrideVariable; }
};

} // namespace wgsl::ast