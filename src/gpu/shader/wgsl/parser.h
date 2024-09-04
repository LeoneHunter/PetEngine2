#pragma once
#include "common.h"
#include "lexer.h"
#include "token.h"

#include "ast_expression.h"
#include "ast_variable.h"
#include "ast_attribute.h"

namespace wgsl {

class Program;
class ProgramBuilder;

enum class ParseResult {
    ParseOk,
    ParseError,
    Unmatched,
};

struct ParseError {
    Token tok;
    std::string msg;
};

template <class T>
using Expected = std::expected<T, ParseResult>;

// Parses a wgsl code into an ast tree
// Top-Down resursive descent parser
class Parser {
public:
    Parser(std::string_view code, ProgramBuilder* builder);

    void Parse();

    bool HasErrors() const { return !errors_.empty(); }

    ParseError GetLastError() const {
        return errors_.empty() ? ParseError() : errors_.back();
    }

private:
    struct TypeInfo {
        std::string_view ident;
        DataType type;
    };

    // Functionally a c++ rvalue,
    // i.e. temporary variable without a memory location and simbol name
    struct Value {
        DataType type;
        Token tok;
    };

    struct TemplateParameter {};

private:
    // Variables
    Expected<ast::Variable*> GlobalVariable();
    Expected<ast::Variable*> ValueDecl();
    Expected<ast::Variable*> ConstValueDecl();
    Expected<ast::Variable*> OverrideValueDecl(
        const std::vector<ast::Attribute*>& attributes);
    Expected<ast::Variable*> VarDecl(
        const std::vector<ast::Attribute*>& attributes);
    Expected<ast::Attribute*> Attribute();

    // Expressions
    Expected<ast::Expression*> Expression();
    Expected<ast::Expression*> UnaryExpr();
    Expected<ast::Expression*> PrimaryExpr();
    Expected<ast::Expression*> ComponentSwizzleExpr(ast::Expression* lhs);

    Expected<ast::IntLiteralExpression*> IntLiteralExpr();
    Expected<ast::FloatLiteralExpression*> FloatLiteralExpr();
    Expected<ast::BoolLiteralExpression*> BoolLiteralExpr();

    // Types
    Expected<TypeInfo> TypeIdent();
    Expected<TypeInfo> TypeGenerator();

private:
    template <class... Args>
    std::unexpected<ParseResult> Unexpected(std::format_string<Args...> fmt,
                                       Args&&... args) {
        return Unexpected(std::format(fmt, std::forward<Args>(args)...));
    }

    std::unexpected<ParseResult> Unexpected(const std::string& msg);
    std::unexpected<ParseResult> Unexpected(ParseResult error);
    std::unexpected<ParseResult> Unmatched();

    template <class... T>
    bool PeekAny(T... tokens) {
        return (Peek(tokens) || ...);
    }

    // Try match with current token
    bool Peek(Token::Kind kind);
    Token Peek() { return token_; }
    // Try to match current token, advance if matched
    bool Expect(Token::Kind kind);
    // Move to the next token (after Peek())
    Token Advance();
    // Get last token (before Next())
    Token GetLastToken() { return lastToken_; }

private:
    bool ShouldExit() { return IsEof(); }
    bool IsEof() const { return token_.kind == Token::Kind::EOF; }

private:
    Lexer lexer_;
    Token token_;
    Token lastToken_;
    std::vector<ParseError> errors_;
    ProgramBuilder* builder_;
};

}  //    namespace wgsl