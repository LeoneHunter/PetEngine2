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
class SymbolTable;

using ParameterList = ProgramList<const Parameter*>;
using StatementList = ProgramList<const Statement*>;

// A Function with a scope
class Function : public Symbol {
public:
    const std::string_view name;
    const AttributeList attributes;
    const ParameterList parameters;
    const Type* retType;
    const AttributeList retAttributes;
    StatementList body;

    SymbolTable* symbolTable;

    constexpr static inline auto kStaticType = NodeType::Function;

    Function(SourceLoc loc,
             SymbolTable* symbolTable,
             std::string_view name,
             AttributeList&& attributes,
             ParameterList&& params,
             const ast::Type* retType,
             AttributeList&& retAttributes)
        : Symbol(loc, kStaticType)
        , name(name)
        , symbolTable(symbolTable)
        , attributes(attributes)
        , parameters(params)
        , retType(retType)
        , retAttributes(retAttributes) {}

    void AddStatement(const ast::Statement* statement) {
        DASSERT(statement);
        body.push_back(statement);
    }
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