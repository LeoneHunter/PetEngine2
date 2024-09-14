#pragma once
#include "ast_attribute.h"
#include "common.h"
#include "lexer.h"
#include "token.h"

namespace wgsl {

namespace ast {

class Variable;
class Expression;
class Attribute;
class Parameter;
class Function;
class IntLiteralExpression;
class BoolLiteralExpression;
class FloatLiteralExpression;
class Struct;

}  // namespace ast

class Program;
class ProgramBuilder;

using ExpressionList = std::vector<const ast::Expression*>;

// Helper to be used with std::expected when no return value is needed
// Could be in three states: Ok, Unmatched, Errored
struct Void {};
using ExpectedVoid = Expected<Void>;

// Parsed template elaborated identifier
// : ident ( '<' template_arg_expression ( ',' expression )* ',' ? '>' )?
struct Ident {
    SourceLoc loc;
    std::string_view name;
    ExpressionList templateList;
};

// Parses a wgsl code into an ast tree
// Top-Down resursive descent parser
class Parser {
public:
    Parser(std::string_view code, ProgramBuilder* builder);
    void Parse();

    std::string_view GetLine(uint32_t line);

private:
    ExpectedVoid GlobalDecl();

    // Statements
    ExpectedVoid Statement();
    ExpectedVoid CompoundStatement();

    ExpectedVoid ConstValueDecl();
    ExpectedVoid OverrideValueDecl(ast::AttributeList& attributes);
    ExpectedVoid VariableDecl(ast::AttributeList& attributes);

    ExpectedVoid FunctionDecl(ast::AttributeList& attributes);
    ExpectedVoid StructDecl();
    Expected<const ast::Parameter*> ParameterDecl();

    // Expressions
    Expected<const ast::Expression*> Expression(bool inTemplate);
    Expected<const ast::Expression*> UnaryExpr();
    Expected<const ast::Expression*> PrimaryExpr();
    Expected<const ast::Expression*> ComponentSwizzleExpr(
        const ast::Expression* lhs);
    Expected<const ast::Expression*> IdentExpression();

    Expected<const ast::IntLiteralExpression*> IntLiteralExpr();
    Expected<const ast::FloatLiteralExpression*> FloatLiteralExpr();
    Expected<const ast::BoolLiteralExpression*> BoolLiteralExpr();

    Expected<const ast::Attribute*> Attribute();
    Expected<Ident> TemplatedIdent();

private:
    template <class... Args>
    std::unexpected<ErrorCode> Unexpected(ErrorCode code,
                                          std::format_string<Args...> fmt,
                                          Args&&... args) {
        return Unexpected(code, std::format(fmt, std::forward<Args>(args)...));
    }

    std::unexpected<ErrorCode> Unexpected(ErrorCode code,
                                          const std::string& msg);
    std::unexpected<ErrorCode> Unexpected(ErrorCode err);
    std::unexpected<ErrorCode> Unexpected(Token::Kind kind);
    std::unexpected<ErrorCode> Unmatched();

    ExpectedVoid Ok() { return Void(); }

    template <class... T>
    bool PeekAny(T... tokens) {
        return (Peek(tokens) || ...);
    }

    // Try match with current token
    bool Peek(Token::Kind kind);
    bool Peek(Keyword Keyword);
    Token Peek() { return token_; }
    // Try to match current token, advance if matched
    bool Expect(Token::Kind kind);
    bool Expect(Keyword keyword);
    // Move to the next token (after Peek())
    Token Advance();
    // Get last token (before Next())
    Token GetLastToken() { return lastToken_; }

private:
    bool ShouldExit();
    bool IsEof() const { return token_.kind == Token::Kind::EOF; }

private:
    Lexer lexer_;
    Token token_;
    Token lastToken_;
    ProgramBuilder* builder_;
};

}  //    namespace wgsl