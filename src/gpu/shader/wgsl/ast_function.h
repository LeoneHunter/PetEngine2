#pragma once
#include "ast_attribute.h"
#include "ast_node.h"
#include "ast_variable.h"
#include "program_alloc.h"

namespace wgsl {

class FunctionScope;

namespace ast {

class Parameter;
class Statement;
class ScopedStatement;

using ParameterList = ProgramList<const Parameter*>;

// A Function with a scope
class Function : public Symbol {
public:
    const std::string_view name;
    const AttributeList attributes;
    const ParameterList parameters;
    const Type* retType;
    const AttributeList retAttributes;
    ScopedStatement* body;

    constexpr static inline auto kStaticType = NodeType::Function;

    Function(SourceLoc loc,
             ScopedStatement* scope,
             std::string_view name,
             AttributeList&& attributes,
             ParameterList&& params,
             const ast::Type* retType,
             AttributeList&& retAttributes)
        : Symbol(loc, kStaticType)
        , name(name)
        , body(scope)
        , attributes(attributes)
        , parameters(params)
        , retType(retType)
        , retAttributes(retAttributes) {}
};

// Function parameters
class Parameter final : public Variable {
public:
    constexpr static inline auto kStaticType = NodeType::Parameter;

    Parameter(SourceLoc loc,
              std::string_view ident,
              const Type* type,
              AttributeList&& attributes)
        : Variable(loc,
                   kStaticType,
                   ident,
                   type,
                   std::move(attributes),
                   nullptr) {}
};


}  // namespace ast
}  // namespace wgsl