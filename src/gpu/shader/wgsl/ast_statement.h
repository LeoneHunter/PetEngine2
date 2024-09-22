#pragma once
#include "ast_node.h"
#include "program_alloc.h"

namespace wgsl {

class GlobalScope;
class FunctionScope;
class SymbolTable;

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
// Has a SymbolTable (hash table) with declared symbols in that scope
class ScopedStatement : public Statement {
public:
    SymbolTable* symbols;
    ProgramList<Statement*> statements;

    constexpr static auto kStaticType = NodeType::ScopedStatement;

    ScopedStatement(SourceLoc loc, SymbolTable* symbols)
        : Statement(loc, kStaticType), symbols(symbols) {}

protected:
    ScopedStatement(SourceLoc loc, NodeType nodeType, SymbolTable* symbols)
        : Statement(loc, nodeType | kStaticType), symbols(symbols) {}
};

// Statement group wrapped in parens { ... }
class CompoundStatement final : public ScopedStatement {
public:
    constexpr static auto kStaticType = NodeType::CompoundStatement;

    CompoundStatement(SourceLoc loc, SymbolTable* symbols)
        : ScopedStatement(loc, kStaticType, symbols) {}
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

// If else clause chain
class IfStatement : public ScopedStatement {
public:
    const Expression* expr;
    // Another IfStatement or CompoundStatement for else
    ScopedStatement* next;

    constexpr static auto kStaticType = NodeType::IfStatement;

    IfStatement(SourceLoc loc, const Expression* expr, SymbolTable* symbols)
        : ScopedStatement(loc, kStaticType, symbols)
        , expr(expr)
        , next(nullptr) {}

    void AppendElse(ScopedStatement* clause) {
        DASSERT(!next);
        next = clause;
    }
};

}  // namespace ast
}  // namespace wgsl