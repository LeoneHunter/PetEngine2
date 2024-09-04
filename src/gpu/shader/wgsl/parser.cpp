#include "parser.h"
#include "program_builder.h"
#include "program.h"

#include <functional>

namespace wgsl {

// Expect the next token to be |tok| else return unmatched code
#define EXPECT_MATCH(tok)       \
    do {                        \
        if (!Peek(tok)) {       \
            return Unmatched(); \
        }                       \
    } while (false)

#define EXPECT_MATCH_ANY(...)        \
    do {                             \
        if (!PeekAny(__VA_ARGS__)) { \
            return Unmatched();      \
        }                            \
    } while (false)

// Expect the next token to be |tok| else return error code
#define EXPECT(tok)                                \
    do {                                           \
        if (Peek(tok)) {                           \
            Advance();                             \
        } else {                                   \
            return Unexpected("Unexpected token"); \
        }                                          \
    } while (false)

// Expect the next optional token to be |tok|
#define EXPECT_OPT(tok)  \
    do {                 \
        if (Peek(tok)) { \
            Advance();   \
        }                \
    } while (false)

// Expect |expr| returning ast node
#define EXPECT_OK(node, expr)                  \
    do {                                       \
        if (auto result = expr) {              \
            node = *result;                    \
        } else {                               \
            return Unexpected(result.error()); \
        }                                      \
    } while (false)

#define UNWRAP_IF_OK(node, expr)  \
    do {                          \
        if (auto result = expr) { \
            node = *result;       \
        }                         \
    } while (false)

#define RET_IF_OK(expr)           \
    do {                          \
        if (auto result = expr) { \
            return *result;       \
        }                         \
    } while (false)

using Tok = wgsl::Token::Kind;

// ================================================================//

Parser::Parser(std::string_view code, ProgramBuilder* builder)
    : lexer_(code), builder_(builder) {}

// global_decl:
// | attribute * 'fn' ident
//     '(' ( attribute * ident ':' type_specifier ( ',' param )* ',' ? )? ')'
//     ( '->' attribute * ident template_elaborated_ident.post.ident )?
//     attribute * '{' statement * '}'
// | attribute * 'var' ( '<' expression ( ',' expression )* ',' ? '>' )?
//     optionally_typed_ident ( '=' expression )? ';'
// | global_value_decl ';'
// | ';'
// | 'alias' ident '=' ident template_elaborated_ident.post.ident ';'
// | 'const_assert' expression ';'
// | 'struct' ident '{'
//      attribute * member_ident ':' type_specifier
//      ( ',' attribute * member_ident ':' type_specifier )* ',' ? '}'
void Parser::Parse() {
    Advance();
    while (!ShouldExit()) {
        ast::Variable* var = nullptr;
        UNWRAP_IF_OK(var, GlobalVariable());
        builder_->PushGlobalDecl(var);
    }
}

//==========================================================================//

// global_variable_decl :
//   attribute* variable_decl ( '=' expression ) ?
Expected<ast::Variable*> Parser::GlobalVariable() {
    std::vector<ast::Attribute*> attributes;
    // '@' attribute
    while (Peek(Tok::Attr)) {
        ast::Attribute* attr = nullptr;
        UNWRAP_IF_OK(attr, Attribute());
        attributes.push_back(attr);
    }
    // variable_decl
    ast::Variable* var = nullptr;
    UNWRAP_IF_OK(var, VarDecl(attributes));
    // TODO: Handle 'const' and 'override'
}

// global_value_decl:
//   | attribute* 'override' optionally_typed_ident ( '=' expression )?
//   | 'const' optionally_typed_ident '=' expression
Expected<ast::Variable*> Parser::ValueDecl() {
    // attributes
    std::vector<ast::Attribute*> attributes;
    while (Peek(Tok::Attr)) {
        if(auto attr = Attribute()) {
            attributes.push_back(*attr);
        }
    }
    // const | override
    RET_IF_OK(OverrideValueDecl(attributes));
    // TODO: const may not have attributes


    RET_IF_OK(ConstValueDecl());
    if (!attributes.empty()) {
        return Unexpected("'const' or 'override' expected");
    }
}

// 'const' optionally_typed_ident '=' expression
Expected<ast::Variable*> Parser::ConstValueDecl() {
    if (Peek().GetString() != "const") {
        return Unmatched();
    }
    Location startLoc = Advance().loc;
    Location typeLoc = Peek().loc;
    TypeInfo typeInfo;
    EXPECT_OK(typeInfo, TypeIdent());
    ast::Expression* initializer = nullptr;
    EXPECT_OK(initializer, Expression());
    auto loc = LocationRange(startLoc,
                             initializer ? initializer->GetLocEnd() : typeLoc);
    return builder_->CreateConstVar(loc, std::string(typeInfo.ident),
                                    initializer);
}

Expected<ast::Variable*> Parser::OverrideValueDecl(
    const std::vector<ast::Attribute*>& attributes) {
    return Unexpected("Unimplemented");
}

// variable_decl :
//   'var' template_list? IDENT (':' type_specifier)?
Expected<ast::Variable*> Parser::VarDecl(
    const std::vector<ast::Attribute*>& attributes) {
    // 'var'
    EXPECT_MATCH(Tok::Keyword);
    if (Peek().GetString() != "var") {
        return Unmatched();
    }
    EXPECT(Tok::Ident);
    Token ident = GetLastToken();
    // Try to find the symbol
    // Parse template list
    TypeInfo typeInfo;
    UNWRAP_IF_OK(typeInfo, TypeGenerator());
    TypeInfo type;
    UNWRAP_IF_OK(type, TypeIdent());

    // Initializer: '=' expression
    ast::Expression* initializer = nullptr;
    UNWRAP_IF_OK(initializer, Expression());

    // TODO: Create a variable node
}

// ident template_list?
Expected<Parser::TypeInfo> Parser::TypeIdent() {
    EXPECT(Tok::Ident);
    Token tok = GetLastToken();
    // Type name
    TypeInfo typeInfo;
    std::string_view typeName = tok.GetString();
    if (typeName == "u32") {
        typeInfo.type = DataType::U32;
    } else if (typeName == "u32") {
        typeInfo.type = DataType::I32;
    }
    // Template
}

// vec[234]<T> | matCxR<T> | atomic<T> | array<E, N?> | ptr<T>
Expected<Parser::TypeInfo> Parser::TypeGenerator() {
    return Unexpected("Unimplemented");
}

Expected<ast::Expression*> Parser::Expression() {
    ast::Expression* unary = nullptr;
    EXPECT_OK(unary, UnaryExpr());
    // ';'
    if (Expect(Tok::Semicolon)) {
        return unary;
    }
    // binary operator
    using BinaryOp = ast::BinaryExpression::OpCode;
    auto op = BinaryOp::Add;

    Token tok = Peek();
    switch (tok.kind) {
        // Arithmetic
        case Tok::Mul: op = BinaryOp::Mul; break;
        case Tok::Div: op = BinaryOp::Div; break;
        case Tok::Plus: op = BinaryOp::Add; break;
        case Tok::Minus: op = BinaryOp::Sub; break;
        case Tok::Mod: op = BinaryOp::Remainder; break;
        // Relation
        case Tok::LessThan: op = BinaryOp::LessThan; break;
        case Tok::GreaterThan: op = BinaryOp::GreaterThan; break;
        case Tok::LessThanEqual: op = BinaryOp::LessThanEqual; break;
        case Tok::GreaterThanEqual: op = BinaryOp::GreaterThanEqual; break;
        case Tok::Equal: op = BinaryOp::Equal; break;
        case Tok::NotEqual: op = BinaryOp::NotEqual; break;
        // Logic
        case Tok::AndAnd: op = BinaryOp::And; break;
        case Tok::OrOr: op = BinaryOp::Or; break;
        // Bitwise
        case Tok::And: op = BinaryOp::BitwiseAnd; break;
        case Tok::Or: op = BinaryOp::BitwiseOr; break;
        case Tok::Xor: op = BinaryOp::BitwiseXor; break;
        case Tok::LeftShift: op = BinaryOp::BitwiseLeftShift; break;
        case Tok::RightShift: op = BinaryOp::BitwiseRightShift; break;
        default: return Unexpected("Unexpected token {}", tok.kind); break;
    }
    // Expect rhs expression
    ast::Expression* lhs = unary;
    ast::Expression* rhs = nullptr;
    auto res = Expression();
    if (!res && res.error() == ParseResult::Unmatched) {
        return Unexpected("Unexpected token {}", Peek().kind);
    }
    if (!res && res.error() == ParseResult::ParseError) {
        return Unexpected(res.error());
    }
    return builder_->CreateBinaryExpr(
        LocationRange(lhs->GetLocStart(), rhs->GetLocEnd()), lhs, op, rhs);
}

// unary_expression:
//   | primary_expression component_or_swizzle_specifier?
//   | '!' unary_expression
//   | '&' unary_expression
//   | '*' unary_expression
//   | '-' unary_expression
//   | '~' unary_expression
Expected<ast::Expression*> Parser::UnaryExpr() {
    // primary_expression
    if (auto res = PrimaryExpr()) {
        ast::Expression* primary = *res;
        auto res2 = ComponentSwizzleExpr(primary);
        if (res2) {
            return *res2;
        }
        if (res2.error() == ParseResult::Unmatched) {
            return primary;
        }
        return Unexpected(res2.error());
    }
    // Unary operator
    using Op = ast::UnaryExpression::OpCode;
    Token opToken = Advance();
    std::optional<Op> op;

    switch (opToken.kind) {
        case Tok::Negation: op = Op::Negation; break;
        case Tok::Tilde: op = Op::BitwiseNot; break;
        case Tok::And: op = Op::Address; break;
        case Tok::Minus: op = Op::Minus; break;
    }
    if (op) {
        ast::Expression* rhs = nullptr;
        EXPECT_OK(rhs, UnaryExpr());
        return builder_->CreateUnaryExpr(
            LocationRange(opToken.loc, rhs->GetLocEnd()), *op, rhs);
    }
    return Unmatched();
}

// primary_expression:
//   | ident template_elaborated_ident.post.ident
//   | ident template_elaborated_ident.post.ident argument_expression_list
//   | literal
//   | '(' expression ')'
Expected<ast::Expression*> Parser::PrimaryExpr() {
    // literal
    RET_IF_OK(IntLiteralExpr());
    RET_IF_OK(FloatLiteralExpr());
    RET_IF_OK(BoolLiteralExpr());
    // '(' expression ')'
    if (Expect(Tok::OpenParen)) {
        ast::Expression* expr = nullptr;
        UNWRAP_IF_OK(expr, Expression());
        // TODO: Handle empty expr: '()'
        EXPECT(Tok::CloseParen);
        return expr;
    }
    // ident template_list
    if (Peek(Tok::Ident)) {
        Token tok = Advance();
        // TODO: Find symbol
    }
    return Unmatched();
}

// component_or_swizzle_specifier:
//   | '.' member_ident component_or_swizzle_specifier ?
//   | '.' swizzle_name component_or_swizzle_specifier ?
//   | '[' expression ']' component_or_swizzle_specifier ?
Expected<ast::Expression*> wgsl::Parser::ComponentSwizzleExpr(
    ast::Expression* lhs) {
    if (Peek(Tok::Period)) {
        return Unexpected("Unimplemented");
    }
    if (Peek(Tok::OpenBracket)) {
        return Unexpected("Unimplemented");
    }
    return Unmatched();
}

Expected<ast::IntLiteralExpression*> Parser::IntLiteralExpr() {
    if (PeekAny(Tok::LitAbstrInt, Tok::LitInt, Tok::LitUint)) {
        Token tok = Advance();

        using Type = ast::IntLiteralExpression::Type;
        auto type = Type::Abstract;

        if (tok.kind == Tok::LitInt) {
            type = Type::I32;
        } else if (tok.kind == Tok::LitUint) {
            type = Type::U32;
        }
        return builder_->CreateIntLiteralExpr(tok.loc, tok.GetInt(), type);
    }
    return Unmatched();
}

Expected<ast::FloatLiteralExpression*> Parser::FloatLiteralExpr() {
    if (PeekAny(Tok::LitAbstrFloat, Tok::LitFloat, Tok::LitHalf)) {
        Token tok = Advance();

        using Type = ast::FloatLiteralExpression::Type;
        auto type = Type::Abstract;

        if (tok.kind == Tok::LitFloat) {
            type = Type::F32;
        } else if (tok.kind == Tok::LitHalf) {
            type = Type::F16;
        }
        return builder_->CreateFloatLiteralExpr(tok.loc, tok.GetDouble(), type);
    }
    return Unmatched();
}

Expected<ast::BoolLiteralExpression*> Parser::BoolLiteralExpr() {
    if (Peek(Tok::LitBool)) {
        Token tok = Advance();
        return builder_->CreateBoolLiteralExpr(tok.loc, tok.GetInt());
    }
    return Unmatched();
}

// attribute :
//   '@' ident_pattern_token argument_expression_list ?
//   | align_attr
//   | binding_attr
//   | blend_src_attr
//   | builtin_attr
//   | const_attr
//   | diagnostic_attr
//   | group_attr
//   | id_attr
//   | interpolate_attr
//   | invariant_attr
//   | location_attr
//   | must_use_attr
//   | size_attr
//   | workgroup_size_attr
//   | vertex_attr
//   | fragment_attr
//   | compute_attr
Expected<ast::Attribute*> Parser::Attribute() {
    EXPECT_MATCH(Tok::Attr);
    EXPECT(Tok::Ident);
    Token ident = GetLastToken();
    // '@' 'align' '(' expression ',' ? ')'
    // positive, pow2, const u32 | i32
    if (ident.Match("align")) {
        EXPECT(Tok::OpenParen);
        ast::Expression* expr = nullptr;
        EXPECT_OK(expr, Expression());
        EXPECT_OPT(Tok::Comma);
        EXPECT(Tok::CloseParen);
        return builder_->CreateAttribute(ident.loc, wgsl::Attribute::Align, expr);
    }
    // '@' 'binding' '(' expression ',' ? ')'
    // positive, const u32 | i32
    if (ident.Match("align")) {
        EXPECT(Tok::OpenParen);
        ast::Expression* expr = nullptr;
        EXPECT_OK(expr, Expression());
        EXPECT_OPT(Tok::Comma);
        EXPECT(Tok::CloseParen);
        return builder_->CreateAttribute(ident.loc, wgsl::Attribute::Binding, expr);
    }
    // @ vertex
    if (ident.Match("vertex")) {
        return builder_->CreateAttribute(ident.loc, wgsl::Attribute::Vertex);
    }
    // @ fragment
    if (ident.Match("fragment")) {
        return builder_->CreateAttribute(ident.loc, wgsl::Attribute::Fragment);
    }
    // @ compute
    if (ident.Match("compute")) {
        return builder_->CreateAttribute(ident.loc, wgsl::Attribute::Compute);
    }
    return Unexpected("'{}' is not a valid attribute name", ident.GetString());
}

//==========================================================================//

std::unexpected<ParseResult> Parser::Unexpected(const std::string& msg) {
    // TODO: Get the location and token from the caller
    // TODO: Emit proper error message (maybe use error code + location)
    errors_.push_back(ParseError{
        .tok = token_,
        .msg = msg,
    });
    return std::unexpected(ParseResult::ParseError);
}

std::unexpected<ParseResult> Parser::Unexpected(ParseResult error) {
    return std::unexpected(ParseResult::ParseError);
}

std::unexpected<ParseResult> Parser::Unmatched() {
    return std::unexpected(ParseResult::Unmatched);
}

bool Parser::Peek(Token::Kind kind) {
    return token_.kind == kind;
}

bool Parser::Expect(Token::Kind kind) {
    if (Peek(kind)) {
        Advance();
        return true;
    }
    return false;
}

Token Parser::Advance() {
    lastToken_ = token_;
    token_ = lexer_.ParseNext();
    return lastToken_;
}

}  // namespace wgsl