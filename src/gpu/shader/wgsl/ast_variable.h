#pragma once
#include "ast_expression.h"
#include "ast_node.h"
#include "ast_type.h"

namespace wgsl::ast {

class Attribute;

// Abstract base for 'var', 'override', 'const'
class Variable : public Node {
public:
    std::string_view ident;
    ast::Type* type;
    std::vector<ast::Attribute*> attributes;
    ast::Expression* initializer;

    constexpr static inline auto kStaticType = NodeType::Variable;

protected:
    Variable(SourceLoc loc,
             NodeType nodeType,
             std::string_view ident,
             Type* type,
             const std::vector<ast::Attribute*>& attributes,
             Expression* initializer)
        : Node(loc, nodeType | kStaticType)
        , ident(ident)
        , type(type)
        , attributes(attributes)
        , initializer(initializer) {}
};

// 'var'
class VarVariable final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::VarVariable;

    VarVariable(SourceLoc loc,
                std::string_view ident,
                Type* type,
                const std::vector<ast::Attribute*>& attributes,
                Expression* initializer)
        : Variable(loc, kStaticType, ident, type, attributes, initializer) {}
};

// 'const'
class ConstVariable final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::ConstVariable;

    ConstVariable(SourceLoc loc,
                  std::string_view ident,
                  Type* type,
                  Expression* initializer)
        : Variable(loc, kStaticType, ident, type, {}, initializer) {}
};

// 'override'
class OverrideVariable final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::OverrideVariable;

    OverrideVariable(SourceLoc loc,
                     std::string_view ident,
                     Type* type,
                     const std::vector<ast::Attribute*>& attributes,
                     Expression* initializer)
        : Variable(loc, kStaticType, ident, type, attributes, initializer) {}
};

}  // namespace wgsl::ast