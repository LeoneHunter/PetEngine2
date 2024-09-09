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

using TemplateList = std::vector<ast::Expression*>;

struct Ident {
    SourceLoc loc;
    std::string_view name;
};

struct TypeInfo {
    Ident ident;
    TemplateList templateList;
};

// Parses a wgsl code into an ast tree
// Top-Down resursive descent parser
class Parser {
public:
    Parser(std::string_view code, ProgramBuilder* builder);
    void Parse();

    std::string_view GetLine(uint32_t line);

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
    Expected<ast::Expression*> IdentExpression();

    Expected<ast::IntLiteralExpression*> IntLiteralExpr();
    Expected<ast::FloatLiteralExpression*> FloatLiteralExpr();
    Expected<ast::BoolLiteralExpression*> BoolLiteralExpr();

    // Types
    Expected<TypeInfo> TypeSpecifier();
    Expected<TypeInfo> TypeGenerator();

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

private:
    Lexer lexer_;
    Token token_;
    Token lastToken_;
    ProgramBuilder* builder_;
};

}  //    namespace wgsl