#pragma once
#include "common.h"
#include "lexer.h"
#include "token.h"

#include "ast_attribute.h"
#include "ast_expression.h"
#include "ast_variable.h"

namespace wgsl {

class Program;
class ProgramBuilder;

enum class ParseResult {
    Ok,
    Error,
    Unmatched,
};

enum class SyntaxError {
    Unknown,
    ExpectedExpr,
    ExpectedType,
    ExpectedIdent,
    ExpectedDecl,
};

constexpr std::string to_string(SyntaxError err) {
    switch (err) {
        case SyntaxError::ExpectedExpr: {
            return "expected and expression";
        }
        case SyntaxError::ExpectedType: {
            return "expected a type specifier";
        }
        case SyntaxError::ExpectedIdent: {
            return "expected an identifier";
        }
        case SyntaxError::ExpectedDecl: {
            return "expected a declaration";
        }
        default: return "";
    }
}

struct ParseError {
    Token tok;
    std::string msg;
};

struct TemplateList {
    std::vector<ast::Expression*> args;
};

struct TypeInfo {
    Token typeIdent;
    TemplateList templateList;
};

template <class T>
using Expected = std::expected<T, ParseResult>;

// Parses a wgsl code into an ast tree
// Top-Down resursive descent parser
class Parser {
public:
    Parser(std::string_view code, ProgramBuilder* builder);
    void Parse();

private:
    // Variables
    Expected<ast::Variable*> GlobalVariable();
    Expected<ast::Variable*> GlobValueDecl(
        const std::vector<ast::Attribute*>& attributes);
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
    Expected<TypeInfo> TypeSpecifier();
    Expected<TypeInfo> TypeGenerator();

private:
    template <class... Args>
    std::unexpected<ParseResult> Unexpected(std::format_string<Args...> fmt,
                                            Args&&... args) {
        return Unexpected(std::format(fmt, std::forward<Args>(args)...));
    }

    std::unexpected<ParseResult> Unexpected(const std::string& msg);
    std::unexpected<ParseResult> Unexpected(SyntaxError err);
    std::unexpected<ParseResult> Unexpected(ParseResult error);
    std::unexpected<ParseResult> Unexpected(Token::Kind kind);
    std::unexpected<ParseResult> Unmatched();

    template <class... T>
    bool PeekAny(T... tokens) {
        return (Peek(tokens) || ...);
    }

    // Try match with current token
    bool Peek(Token::Kind kind);
    bool PeekWith(Token::Kind kind, std::string_view val);
    bool PeekValue(std::string_view val);
    Token Peek() { return token_; }
    // Try to match current token, advance if matched
    bool Expect(Token::Kind kind);
    // Move to the next token (after Peek())
    Token Advance();
    // Get last token (before Next())
    Token GetLastToken() { return lastToken_; }

private:
    bool ShouldExit();
    bool IsEof() const { return token_.kind == Token::Kind::EOF; }

    // Format syntax error message
    std::string CreateErrorMsg(LocationRange loc, const std::string& msg);

private:
    Lexer lexer_;
    Token token_;
    Token lastToken_;
    ProgramBuilder* builder_;
};

}  //    namespace wgsl