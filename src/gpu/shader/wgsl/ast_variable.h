#pragma once
#include "ast_node.h"
#include "ast_type.h"
#include "program_alloc.h"

namespace wgsl::ast {

class Attribute;
class Expression;

// Abstract base for 'var', 'override', 'const'
class Variable : public Symbol {
public:
    const std::string_view ident;
    const ast::Type* type;
    const ast::Expression* initializer;
    const AttributeList attributes;

    constexpr static inline auto kStaticType = NodeType::Variable;

protected:
    Variable(SourceLoc loc,
             NodeType nodeType,
             std::string_view ident,
             const Type* type,
             AttributeList&& attributes,
             const Expression* initializer)
        : Symbol(loc, nodeType | kStaticType)
        , ident(ident)
        , type(type)
        , attributes(attributes)
        , initializer(initializer) {}
};

// 'var'
class VarVariable final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::VarVariable;

    const AddressSpace addressSpace;
    const AccessMode accessMode;

    VarVariable(SourceLoc loc,
                std::string_view ident,
                AddressSpace as,
                AccessMode am,
                const Type* type,
                AttributeList&& attributes,
                const Expression* initializer)
        : Variable(loc,
                   kStaticType,
                   ident,
                   type,
                   std::move(attributes),
                   initializer)
        , addressSpace(as)
        , accessMode(am) {}
};

// 'const'
class ConstVariable final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::ConstVariable;

    ConstVariable(SourceLoc loc,
                  std::string_view ident,
                  const Type* type,
                  const Expression* initializer)
        : Variable(loc, kStaticType, ident, type, {}, initializer) {}
};

// 'override'
// class OverrideVariable final : public Variable {
// public:
//     constexpr static inline auto kStaticType = NodeType::OverrideVariable;

//     OverrideVariable(SourceLoc loc,
//                      std::string_view ident,
//                      const Type* type,
//                      const std::vector<ast::Attribute*>& attributes,
//                      const Expression* initializer)
//         : Variable(loc, kStaticType, ident, type, attributes, initializer) {}
// };

}  // namespace wgsl::ast