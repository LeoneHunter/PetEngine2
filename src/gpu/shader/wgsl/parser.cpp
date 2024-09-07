#include "parser.h"
#include "program.h"
#include "program_builder.h"

#include <functional>

namespace wgsl {

// Expect the next token to be |tok| else return unmatched code
#define EXPECT_MATCH(tok)       \
    do {                        \
        if (Peek(tok)) {        \
            Advance();          \
        } else {                \
            return Unmatched(); \
        }                       \
    } while (false)

#define EXPECT_MATCH_ANY(...)       \
    do {                            \
        if (PeekAny(__VA_ARGS__)) { \
            Advance();              \
        } else {                    \
            return Unmatched();     \
        }                           \
    } while (false)

// Expect the next token to be |tok| else return error code
#define EXPECT(tok)                 \
    do {                            \
        if (Peek(tok)) {            \
            Advance();              \
        } else {                    \
            return Unexpected(tok); \
        }                           \
    } while (false)

#define EXPECT_C(tok, code)          \
    do {                             \
        if (Peek(tok)) {             \
            Advance();               \
        } else {                     \
            return Unexpected(code); \
        }                            \
    } while (false)

// Expect the next optional token to be |tok|
#define EXPECT_OPT(tok)  \
    do {                 \
        if (Peek(tok)) { \
            Advance();   \
        }                \
    } while (false)

// Expect a result and unwrap it
#define EXPECT_UNWRAP(node, expr, msg) \
    do {                               \
        auto res = expr;               \
        if (res) {                     \
            node = *res;               \
        } else {                       \
            return Unexpected(msg);    \
        }                              \
    } while (false)

// Expect a result and skip if unmatched
#define EXPECT_UNWRAP_OPT(node, expr)              \
    do {                                           \
        auto res = expr;                           \
        if (res) {                                 \
            node = *res;                           \
        } else if (res.error() == Result::Error) { \
            return Unexpected(res.error());        \
        }                                          \
    } while (false)

// Expect a result and return if ok
#define EXPECT_UNWRAP_RET(expr)                    \
    do {                                           \
        auto res = expr;                           \
        if (res) {                                 \
            return *res;                           \
        } else if (res.error() == Result::Error) { \
            return Unexpected(res.error());        \
        }                                          \
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
        auto res = GlobalVariable();
        if (res) {
            builder_->PushGlobalDecl(*res);
            continue;
        }
        if (res.error() == Result::Error) {
            continue;
        }
        // Empty decl ';'
        if (Expect(Tok::Semicolon)) {
            continue;
        }
        if (Peek(Tok::Reserved)) {
            Unexpected(ErrorCode::IdentReserved);
            continue;
        }
        if (Peek(Tok::Ident) || Peek().IsLiteral()) {
            Unexpected(ErrorCode::ExpectedStorageClass,
                       "expected storage class 'const', 'override' or 'var'");
            continue;
        }
        Unexpected(ErrorCode::ExpectedDecl);
    }
}

std::string_view Parser::GetLine(uint32_t line) {
    return lexer_.GetLine(line);
}

//==========================================================================//

// | global_value_decl ';'
// | attribute * 'var' ( '<' expression ( ',' expression )* ',' ? '>' )?
//     optionally_typed_ident ( '=' expression )? ';'
Expected<ast::Variable*> Parser::GlobalVariable() {
    // attributes
    std::vector<ast::Attribute*> attributes;
    while (Peek(Tok::Attr)) {
        if (auto attr = Attribute()) {
            attributes.push_back(*attr);
        }
    }
    // const override
    ast::Variable* var = nullptr;
    EXPECT_UNWRAP_OPT(var, GlobValueDecl(attributes));
    if (var) {
        EXPECT(Tok::Semicolon);
        return var;
    }
    // var
    EXPECT_UNWRAP_OPT(var, VarDecl(attributes));
    if (var) {
        EXPECT(Tok::Semicolon);
        return var;
    }
    return Unmatched();
}

// global_value_decl:
//   | attribute* 'override' optionally_typed_ident ( '=' expression )?
//   | 'const' optionally_typed_ident '=' expression
Expected<ast::Variable*> Parser::GlobValueDecl(
    const std::vector<ast::Attribute*>& attributes) {
    // const | override
    ast::Variable* var = nullptr;
    EXPECT_UNWRAP_OPT(var, OverrideValueDecl(attributes));
    EXPECT_UNWRAP_OPT(var, ConstValueDecl());
    if (var && !attributes.empty()) {
        return Unexpected(ErrorCode::ConstNoAttr);
    }
    return var;
}

// 'const' ident ( ':' type_specifier )? '=' expression
Expected<ast::Variable*> Parser::ConstValueDecl() {
    // 'const'
    if (!PeekWith(Tok::Keyword, "const")) {
        return Unmatched();
    }
    SourceLoc loc = Advance().loc;
    // ident
    EXPECT(Tok::Ident);
    Token ident = GetLastToken();
    // type_specifier
    SourceLoc typeLoc = Peek().loc;
    std::optional<TypeInfo> typeInfo;
    if (Expect(Tok::Colon)) {
        EXPECT_UNWRAP(typeInfo, TypeSpecifier(), ErrorCode::ExpectedType);
    }
    // '=' expression
    EXPECT_C(Tok::Equal, ErrorCode::ConstDeclNoInitializer);
    ast::Expression* initializer = nullptr;
    EXPECT_UNWRAP(initializer, Expression(), ErrorCode::ExpectedExpr);
    // auto loc = Location(startLoc,
    //                          initializer ? initializer->GetLocEnd() :
    //                          typeLoc);
    return builder_->CreateConstVar(loc, Ident(ident.loc, ident.Source()),
                                    typeInfo, initializer);
}

// 'override'
Expected<ast::Variable*> Parser::OverrideValueDecl(
    const std::vector<ast::Attribute*>& attributes) {
    if (!PeekWith(Tok::Keyword, "override")) {
        return Unmatched();
    }
    return Unexpected(ErrorCode::Unimplemented,
                      "'override' variables not implemented");
}

// 'var' template_list? optionally_typed_ident ( '=' expression )?
Expected<ast::Variable*> Parser::VarDecl(
    const std::vector<ast::Attribute*>& attributes) {
    // 'var'
    if (!PeekWith(Tok::Keyword, "var")) {
        return Unmatched();
    }
    SourceLoc loc = Advance().loc;
    // TODO: parse var template
    EXPECT(Tok::Ident);
    Token ident = GetLastToken();
    // ':'
    std::optional<TypeInfo> typeInfo;
    if (Expect(Tok::Colon)) {
        EXPECT_UNWRAP(typeInfo, TypeSpecifier(), ErrorCode::ExpectedType);
    }
    // Initializer: '=' expression
    ast::Expression* initializer = nullptr;
    if (Expect(Tok::Equal)) {
        EXPECT_UNWRAP(initializer, Expression(), ErrorCode::ExpectedExpr);
    }
    return builder_->CreateVar(loc, Ident(ident.loc, ident.Source()), {},
                               typeInfo, attributes, initializer);
}

// type_specifier:
//   ident ( '<' expression ( ',' expression )* ','? '>' )?
Expected<TypeInfo> Parser::TypeSpecifier() {
    EXPECT_MATCH(Tok::Ident);
    Token tok = GetLastToken();
    // Template parameter list: '<'
    if (Peek(Tok::LessThan)) {
        return Unexpected(ErrorCode::Unimplemented,
                          "templates not implemented");
    }
    return TypeInfo{Ident{tok.loc, tok.Source()}};
}

// vec[234]<T> | matCxR<T> | atomic<T> | array<E, N?> | ptr<T>
Expected<TypeInfo> Parser::TypeGenerator() {
    return Unexpected(ErrorCode::Unimplemented, "templates not implemented");
}

// unary_expression | relational_expr ...
Expected<ast::Expression*> Parser::Expression() {
    ast::Expression* unary = nullptr;
    EXPECT_UNWRAP(unary, UnaryExpr(), ErrorCode::ExpectedExpr);
    // binary operator
    using BinaryOp = ast::BinaryExpression::OpCode;
    std::optional<BinaryOp> op;

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
    }
    if (!op) {
        return unary;
    }
    Advance();
    // Expect rhs expression
    ast::Expression* lhs = unary;
    ast::Expression* rhs = nullptr;
    EXPECT_UNWRAP(rhs, Expression(), ErrorCode::ExpectedExpr);
    return builder_->CreateBinaryExpr(SourceLoc(lhs->GetLoc(), rhs->GetLoc()),
                                      lhs, *op, rhs);
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
    ast::Expression* primary = nullptr;
    EXPECT_UNWRAP_OPT(primary, PrimaryExpr());
    if (primary) {
        auto res2 = ComponentSwizzleExpr(primary);
        if (res2) {
            return *res2;
        }
        if (res2.error() == Result::Unmatched) {
            return primary;
        }
        return Unexpected(res2.error());
    }
    // Unary operator
    using Op = ast::UnaryExpression::OpCode;
    Token opToken = Peek();
    std::optional<Op> op;

    switch (opToken.kind) {
        case Tok::Negation: op = Op::Negation; break;
        case Tok::Tilde: op = Op::BitwiseNot; break;
        case Tok::Minus: op = Op::Minus; break;
        case Tok::And:
        case Tok::Mul:
            return Unexpected(ErrorCode::Unimplemented,
                              "pointers are unsupported");
    }
    if (op) {
        ast::Expression* rhs = nullptr;
        EXPECT_UNWRAP(rhs, UnaryExpr(), ErrorCode::ExpectedExpr);
        return builder_->CreateUnaryExpr(SourceLoc(opToken.loc, rhs->GetLoc()),
                                         *op, rhs);
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
    EXPECT_UNWRAP_RET(IntLiteralExpr());
    EXPECT_UNWRAP_RET(FloatLiteralExpr());
    EXPECT_UNWRAP_RET(BoolLiteralExpr());
    // '(' expression ')'
    if (Expect(Tok::OpenParen)) {
        ast::Expression* expr = nullptr;
        EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
        EXPECT(Tok::CloseParen);
        return expr;
    }
    // ident template_list
    EXPECT_UNWRAP_RET(IdentExpression());
    return Unmatched();
}

// component_or_swizzle_specifier:
//   | '.' member_ident component_or_swizzle_specifier ?
//   | '.' swizzle_name component_or_swizzle_specifier ?
//   | '[' expression ']' component_or_swizzle_specifier ?
Expected<ast::Expression*> wgsl::Parser::ComponentSwizzleExpr(
    ast::Expression* lhs) {
    if (Peek(Tok::Period)) {
        return Unexpected(ErrorCode::Unimplemented);
    }
    if (Peek(Tok::OpenBracket)) {
        return Unexpected(ErrorCode::Unimplemented);
    }
    return Unmatched();
}

// | ident template_elaborated_ident.post.ident
// | ident template_elaborated_ident.post.ident argument_expression_list
Expected<ast::Expression*> Parser::IdentExpression() {
    EXPECT_MATCH(Tok::Ident);
    const auto ident = Ident(GetLastToken().loc, GetLastToken().Source());
    // template list '<'
    if (Expect(Tok::LessThan)) {
        return Unexpected(ErrorCode::Unimplemented);
    }
    return builder_->CreateIdentExpr(ident);
}

Expected<ast::IntLiteralExpression*> Parser::IntLiteralExpr() {
    if (PeekAny(Tok::LitAbstrInt, Tok::LitInt, Tok::LitUint)) {
        Token tok = Advance();

        using Type = ast::Type::Kind;
        auto type = Type::AbstrInt;

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

        using Type = ast::Type::Kind;
        auto type = Type::AbstrFloat;

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
        EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
        EXPECT_OPT(Tok::Comma);
        EXPECT(Tok::CloseParen);
        return builder_->CreateAttribute(ident.loc, wgsl::AttributeName::Align,
                                         expr);
    }
    // '@' 'binding' '(' expression ',' ? ')'
    // positive, const u32 | i32
    if (ident.Match("align")) {
        EXPECT(Tok::OpenParen);
        ast::Expression* expr = nullptr;
        EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
        EXPECT_OPT(Tok::Comma);
        EXPECT(Tok::CloseParen);
        return builder_->CreateAttribute(ident.loc,
                                         wgsl::AttributeName::Binding, expr);
    }
    // @ vertex
    if (ident.Match("vertex")) {
        return builder_->CreateAttribute(ident.loc,
                                         wgsl::AttributeName::Vertex);
    }
    // @ fragment
    if (ident.Match("fragment")) {
        return builder_->CreateAttribute(ident.loc,
                                         wgsl::AttributeName::Fragment);
    }
    // @ compute
    if (ident.Match("compute")) {
        return builder_->CreateAttribute(ident.loc,
                                         wgsl::AttributeName::Compute);
    }
    return Unexpected(ErrorCode::InvalidAttribute,
                      "'{}' is not a valid attribute name", ident.Source());
}

//==========================================================================//

std::unexpected<Result> Parser::Unexpected(ErrorCode code,
                                           const std::string& msg) {
    builder_->AddError(token_.loc, code, msg);
    return std::unexpected(Result::Error);
}

std::unexpected<Result> Parser::Unexpected(ErrorCode code) {
    return Unexpected(code, std::string(ErrorCodeDefaultMsg(code)));
}

std::unexpected<Result> Parser::Unexpected(Result res) {
    return std::unexpected(res);
}

std::unexpected<Result> Parser::Unexpected(Token::Kind kind) {
    auto msg = std::format("expected a {}", TokenToStringDiag(kind));
    builder_->AddError(token_.loc, ErrorCode::UnexpectedToken, msg);
    return std::unexpected(Result::Error);
}

std::unexpected<Result> Parser::Unmatched() {
    return std::unexpected(Result::Unmatched);
}

bool Parser::Peek(Token::Kind kind) {
    return token_.kind == kind;
}

bool Parser::PeekWith(Token::Kind kind, std::string_view val) {
    return token_.kind == kind && token_.Source() == val;
}

bool Parser::PeekValue(std::string_view val) {
    return token_.Source() == val;
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

bool Parser::ShouldExit() {
    return IsEof() || builder_->ShouldStopParsing();
}

}  // namespace wgsl