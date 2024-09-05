#pragma once
#include "ast_expression.h"
#include "ast_node.h"

namespace wgsl::ast {

class Attribute;

// Abstract base for 'var', 'override', 'const'
class Variable : public Node {
public:
    std::string_view ident;
    // TODO: Add ast::Type* type;
    std::vector<ast::Attribute*> attributes;
    ast::Expression* initializer;

    constexpr static inline auto kStaticType = NodeType::Variable;

protected:
    Variable(LocationRange loc,
             NodeType type,
             std::string_view ident,
             const std::vector<ast::Attribute*>& attributes,
             Expression* initializer)
        : Node(loc, type | kStaticType)
        , ident(ident)
        , attributes(attributes)
        , initializer(initializer) {}
};

// 'var'
class VarVariable final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::VarVariable;

    VarVariable(LocationRange loc,
                std::string_view ident,
                const std::vector<ast::Attribute*>& attributes,
                Expression* initializer)
        : Variable(loc, kStaticType, ident, attributes, initializer) {}
};

// 'const'
class ConstVariable final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::ConstVariable;

    ConstVariable(LocationRange loc,
                  std::string_view ident,
                  Expression* initializer)
        : Variable(loc, kStaticType, ident, {}, initializer) {}
};

// 'override'
class OverrideVariable final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::OverrideVariable;

    OverrideVariable(LocationRange loc,
                     std::string_view ident,
                     const std::vector<ast::Attribute*>& attributes,
                     Expression* initializer)
        : Variable(loc, kStaticType, ident, attributes, initializer) {}
};

}  // namespace wgsl::ast