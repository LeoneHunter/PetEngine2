#pragma once
#include "ast_node.h"
#include "program_alloc.h"

namespace wgsl {

class GlobalScope;
class FunctionScope;
class BlockScope;

namespace ast {

class Expression;
class VarVariable;

// Base class for statements inside functions
class Statement : public Node {
public:
    constexpr static auto kStaticType = NodeType::Statement;

protected:
    Statement(SourceLoc loc, NodeType nodeType)
        : Node(loc, nodeType | kStaticType) {}
};

// Statement with scope: if, while, for, compound, etc
class ScopedStatement : public Statement {
public:
    BlockScope* scope;
    ProgramList<Statement*> statements;

    constexpr static auto kStaticType = NodeType::ScopedStatement;

protected:
    ScopedStatement(SourceLoc loc, NodeType nodeType, BlockScope* scope)
        : Statement(loc, nodeType | kStaticType) {}
};

// Statement group wrapped in parens { ... }
class CompoundStatement final : public ScopedStatement {
public:
    constexpr static auto kStaticType = NodeType::CompoundStatement;

    CompoundStatement(SourceLoc loc, BlockScope* scope)
        : ScopedStatement(loc, kStaticType, scope) {}
};

// return expression;
class ReturnStatement : public Statement {
public:
    const Expression* expr;

    constexpr static auto kStaticType = NodeType::ReturnStatement;

    ReturnStatement(SourceLoc loc, const Expression* expr)
        : Statement(loc, kStaticType) {}
};

class AssignStatement : public Statement {
public:
    const VarVariable* var;
    const Expression* expr;

    constexpr static auto kStaticType = NodeType::AssignStatement;

    AssignStatement(SourceLoc loc,
                    const VarVariable* var,
                    const Expression* expr)
        : Statement(loc, kStaticType), var(var), expr(expr) {}
};

}  // namespace ast
}  // namespace wgsl