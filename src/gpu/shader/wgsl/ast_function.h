#pragma once
#include "ast_node.h"
#include "ast_attribute.h"
#include "program_alloc.h"

namespace wgsl::ast {

class Parameter;
using ParamList = ProgramVector<const ast::Parameter*>;

class Function : public Symbol {
public:
    const AttributeList attributes;
    const ParamList parameters;

public:
    constexpr static inline auto kStaticType = NodeType::Function;

    Function(SourceLoc loc, AttributeList&& attributes, ParamList&& params)
        : Symbol(loc, kStaticType)
        , attributes(attributes)
        , parameters(parameters) {}

protected:
    Function(NodeType nodeType, ParamList&& params)
        : Symbol(SourceLoc(), nodeType | kStaticType)
        , attributes(attributes)
        , parameters(parameters) {}
};

// Builtin function: interpolate, log
class BuiltinFunction : public Function {
public:

public:
    constexpr static inline auto kStaticType = NodeType::Function;

    BuiltinFunction(): Function(kStaticType, {}) {}
};

}  // namespace wgsl::ast