#include "parser.h"
#include "program.h"
#include "program_builder.h"

#include <functional>

namespace wgsl {

// Check the condition, if false returns
#define EXPECT_COND(COND, ...)              \
    do {                                    \
        if (!(COND)) {                      \
            return Unexpected(__VA_ARGS__); \
        }                                   \
    } while (false)


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
#define EXPECT_TOK(tok)             \
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
#define EXPECT_UNWRAP_OPT(node, expr)                     \
    do {                                                  \
        auto res = expr;                                  \
        if (res) {                                        \
            node = *res;                                  \
        } else if (res.error() != ErrorCode::Unmatched) { \
            return std::unexpected(res.error());          \
        }                                                 \
    } while (false)

// Expect a result and return if ok
#define EXPECT_UNWRAP_RET(expr)                           \
    do {                                                  \
        auto res = expr;                                  \
        if (res) {                                        \
            return *res;                                  \
        } else if (res.error() != ErrorCode::Unmatched) { \
            return std::unexpected(res.error());          \
        }                                                 \
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
        {
            auto var = GlobalVariable();
            if (var) {
                builder_->PushGlobalDecl(*var);
                continue;
            }
            if (var.error() != ErrorCode::Unmatched) {
                continue;
            }
        }
        {
            auto userStruct = Struct();
            if (userStruct) {
                builder_->PushGlobalDecl(*userStruct);
                continue;
            }
            if (userStruct.error() != ErrorCode::Unmatched) {
                continue;
            }
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
    AttributeList attributes;
    while (Peek(Tok::Attr)) {
        if (auto attr = Attribute()) {
            attributes.push_back(*attr);
        }
    }
    // const override
    ast::Variable* var = nullptr;
    EXPECT_UNWRAP_OPT(var, GlobValueDecl(attributes));
    if (var) {
        EXPECT_TOK(Tok::Semicolon);
        return var;
    }
    // var
    EXPECT_UNWRAP_OPT(var, VarDecl(attributes));
    if (var) {
        EXPECT_TOK(Tok::Semicolon);
        return var;
    }
    return Unmatched();
}

// global_value_decl:
//   | attribute* 'override' optionally_typed_ident ( '=' expression )?
//   | 'const' optionally_typed_ident '=' expression
Expected<ast::Variable*> Parser::GlobValueDecl(ast::AttributeList& attributes) {
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
    EXPECT_TOK(Tok::Ident);
    Token ident = GetLastToken();
    // type_specifier
    SourceLoc typeLoc = Peek().loc;
    std::optional<Ident> typeSpecifier;
    if (Expect(Tok::Colon)) {
        EXPECT_UNWRAP(typeSpecifier, TemplatedIdent(), ErrorCode::ExpectedType);
    }
    // '=' expression
    EXPECT_C(Tok::Equal, ErrorCode::ConstDeclNoInitializer);
    ast::Expression* initializer = nullptr;
    EXPECT_UNWRAP(initializer, Expression(), ErrorCode::ExpectedExpr);
    return builder_->CreateConstVar(loc, Ident(ident.loc, ident.Source()),
                                    typeSpecifier, initializer);
}

// 'override'
Expected<ast::Variable*> Parser::OverrideValueDecl(
    ast::AttributeList& attributes) {
    if (!PeekWith(Tok::Keyword, "override")) {
        return Unmatched();
    }
    return Unexpected(ErrorCode::Unimplemented,
                      "'override' variables not implemented");
}

// 'var' template_list? ident ( : type)? ( '=' expression )?
Expected<ast::Variable*> Parser::VarDecl(ast::AttributeList& attributes) {
    // 'var'
    if (!PeekKeyword("var")) {
        return Unmatched();
    }
    const SourceLoc loc = Advance().loc;
    std::optional<AddressSpace> addrSpace;
    std::optional<AccessMode> accessMode;
    // The address space is specified first, as one of the predeclared address
    //   space enumerants. The access mode is specified second, if present, as
    //   one of the predeclared address mode enumerants.
    // The address space must be specified if the access mode is specified.
    if (Expect(Tok::LessThan)) {
        // ( address_space (, access_mode)? )?
        if (Peek(Tok::Ident)) {
            const Token tok = Advance();
            addrSpace = AddressSpaceFromString(tok.Source());
            EXPECT_COND(addrSpace && addrSpace.value() != AddressSpace::Handle,
                        ErrorCode::InvalidArg);
            // Parse access mode
            if (Expect(Tok::Comma)) {
                const Token tok2 = Advance();
                accessMode = AccessModeFromString(tok2.Source());
                EXPECT_COND(accessMode, ErrorCode::UnexpectedToken,
                            "expected a variable access mode");
            }
        }
        EXPECT_TOK(Tok::GreaterThan);
    }
    // Ident
    Ident ident;
    EXPECT_UNWRAP(ident, TemplatedIdent(), ErrorCode::ExpectedIdent);
    // ':'
    std::optional<Ident> typeSpecifier;
    if (Expect(Tok::Colon)) {
        EXPECT_UNWRAP(typeSpecifier, TemplatedIdent(), ErrorCode::ExpectedType);
    }
    // Initializer: '=' expression
    ast::Expression* initializer = nullptr;
    if (Expect(Tok::Equal)) {
        EXPECT_UNWRAP(initializer, Expression(), ErrorCode::ExpectedExpr);
    }
    return builder_->CreateVar(loc, ident, addrSpace, accessMode, typeSpecifier,
                               attributes, initializer);
    return Unexpected(ErrorCode::Unimplemented);
}

// ident ( '<' expression ( ',' expression )* ','? '>' )?
Expected<Ident> Parser::TemplatedIdent() {
    EXPECT_MATCH(Tok::Ident);
    Token ident = GetLastToken();
    ExpressionList templ;
    // Template parameter list: '<'
    if (Expect(Tok::LessThan)) {
        while (!Expect(Tok::GreaterThan)) {
            ast::Expression* expr = nullptr;
            EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
            templ.push_back(expr);
            EXPECT_OPT(Tok::Comma);
        }
    }
    return Ident{ident.loc, ident.Source(), templ};
}

// unary_expression | relational_expr ...
Expected<ast::Expression*> Parser::Expression() {
    ast::Expression* unary = nullptr;
    EXPECT_UNWRAP(unary, UnaryExpr(), ErrorCode::ExpectedExpr);
    // binary operator
    std::optional<OpCode> op;

    Token tok = Peek();
    switch (tok.kind) {
        // Arithmetic
        case Tok::Mul: op = OpCode::Mul; break;
        case Tok::Div: op = OpCode::Div; break;
        case Tok::Plus: op = OpCode::Add; break;
        case Tok::Minus: op = OpCode::Sub; break;
        case Tok::Mod: op = OpCode::Mod; break;
        // Relation
        case Tok::LessThan: op = OpCode::Less; break;
        case Tok::GreaterThan: op = OpCode::Greater; break;
        case Tok::LessThanEqual: op = OpCode::LessEqual; break;
        case Tok::GreaterThanEqual: op = OpCode::GreaterEqual; break;
        case Tok::Equal: op = OpCode::Equal; break;
        case Tok::NotEqual: op = OpCode::NotEqual; break;
        // Logic
        case Tok::AndAnd: op = OpCode::LogAnd; break;
        case Tok::OrOr: op = OpCode::LogOr; break;
        // Bitwise
        case Tok::And: op = OpCode::BitAnd; break;
        case Tok::Or: op = OpCode::BitOr; break;
        case Tok::Xor: op = OpCode::BitXor; break;
        case Tok::LeftShift: op = OpCode::BitLsh; break;
        case Tok::RightShift: op = OpCode::BitRsh; break;
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
                                      *op, lhs, rhs);
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
        if (res2.error() == ErrorCode::Unmatched) {
            return primary;
        }
        return Unexpected(res2.error());
    }
    // Unary operator
    Token opToken = Peek();
    std::optional<OpCode> op;

    switch (opToken.kind) {
        case Tok::Negation: op = OpCode::LogNot; break;
        case Tok::Tilde: op = OpCode::BitNot; break;
        case Tok::Minus: op = OpCode::Negation; break;
        case Tok::And:
        case Tok::Mul:
            return Unexpected(ErrorCode::Unimplemented,
                              "pointers are unsupported");
    }
    if (op) {
        Advance();
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
    // ident template_list
    EXPECT_UNWRAP_RET(IdentExpression());
    // '(' expression ')'
    if (Expect(Tok::OpenParen)) {
        ast::Expression* expr = nullptr;
        EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
        EXPECT_TOK(Tok::CloseParen);
        return expr;
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
        return Unexpected(ErrorCode::Unimplemented);
    }
    if (Peek(Tok::OpenBracket)) {
        return Unexpected(ErrorCode::Unimplemented);
    }
    return Unmatched();
}

// : ident template_elaborated_ident.post.ident
// | ident template_elaborated_ident.post.ident '(' ( expression ( ','
// expression )* ',' ? )? ')'
Expected<ast::Expression*> Parser::IdentExpression() {
    EXPECT_MATCH(Tok::Ident);
    Token tok = GetLastToken();
    Ident ident;
    ident.loc = tok.loc;
    ident.name = tok.Source();
    // Template parameter list: '<'
    if (Expect(Tok::LessThan)) {
        while (!Expect(Tok::GreaterThan)) {
            ast::Expression* expr = nullptr;
            EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
            ident.templateList.push_back(expr);
            EXPECT_OPT(Tok::Comma);
        }
    }
    // function call
    if (Expect(Tok::OpenParen)) {
        ExpressionList args;
        while (!Expect(Tok::CloseParen)) {
            ast::Expression* expr = nullptr;
            EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
            args.push_back(expr);
            EXPECT_OPT(Tok::Comma);
        }
        return builder_->CreateFnCallExpr(ident, args);
    }
    return builder_->CreateIdentExpr(ident);
}

Expected<ast::IntLiteralExpression*> Parser::IntLiteralExpr() {
    if (PeekAny(Tok::LitAbstrInt, Tok::LitInt, Tok::LitUint)) {
        Token tok = Advance();
        auto type = ScalarKind::Int;
        if (tok.kind == Tok::LitInt) {
            type = ScalarKind::I32;
        } else if (tok.kind == Tok::LitUint) {
            type = ScalarKind::U32;
        }
        return builder_->CreateIntLiteralExpr(tok.loc, tok.GetInt(), type);
    }
    return Unmatched();
}

Expected<ast::FloatLiteralExpression*> Parser::FloatLiteralExpr() {
    if (PeekAny(Tok::LitAbstrFloat, Tok::LitFloat, Tok::LitHalf)) {
        Token tok = Advance();
        auto type = ScalarKind::Float;
        if (tok.kind == Tok::LitFloat) {
            type = ScalarKind::F32;
        } else if (tok.kind == Tok::LitHalf) {
            type = ScalarKind::F16;
        }
        return builder_->CreateFloatLiteralExpr(tok.loc, tok.GetDouble(), type);
    }
    return Unmatched();
}

Expected<ast::BoolLiteralExpression*> Parser::BoolLiteralExpr() {
    if (Peek(Tok::Keyword)) {
        bool value = false;
        Token tok = Peek();
        if (tok.Source() == "true") {
            value = true;
        } else if (tok.Source() == "false") {
            value = false;
        } else {
            return Unmatched();
        }
        Advance();
        return builder_->CreateBoolLiteralExpr(tok.loc, value);
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
    EXPECT_TOK(Tok::Ident);
    const Token ident = GetLastToken();
    // '@' 'align' '(' expression ',' ? ')'
    // positive, pow2, const u32 | i32
    if (ident.Match("align")) {
        EXPECT_TOK(Tok::OpenParen);
        ast::Expression* expr = nullptr;
        EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
        EXPECT_OPT(Tok::Comma);
        EXPECT_TOK(Tok::CloseParen);
        return builder_->CreateAttribute(ident.loc, wgsl::AttributeName::Align,
                                         expr);
    }
    // '@' 'binding' '(' expression ',' ? ')'
    // positive, const u32 | i32
    if (ident.Match("align")) {
        EXPECT_TOK(Tok::OpenParen);
        ast::Expression* expr = nullptr;
        EXPECT_UNWRAP(expr, Expression(), ErrorCode::ExpectedExpr);
        EXPECT_OPT(Tok::Comma);
        EXPECT_TOK(Tok::CloseParen);
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

// global_decl:
//   'struct' ident '{'
//      attribute* ident ':' type_specifier
//      ( ',' attribute* ident ':' type_specifier )* ',' ?
//   '}'
Expected<ast::Struct*> Parser::Struct() {
    if (!PeekKeyword("struct")) {
        return Unmatched();
    }
    Advance();
    EXPECT_TOK(Tok::Ident);
    const Token tok = GetLastToken();
    const Ident structIdent = Ident{tok.loc, tok.Source()};

    EXPECT_TOK(Tok::OpenBrace);
    MemberList members;

    while (!Expect(Tok::CloseBrace)) {
        AttributeList attributes;
        while (Peek(Tok::Attr)) {
            const ast::Attribute* attr = nullptr;
            EXPECT_UNWRAP(attr, Attribute(), ErrorCode::InvalidAttribute);
            attributes.push_back(attr);
        }
        EXPECT_TOK(Tok::Ident);
        const Token memberTok = GetLastToken();
        const Ident memberIdent = Ident{memberTok.loc, memberTok.Source()};
        EXPECT_TOK(Tok::Colon);
        Ident typeSpecifier;
        EXPECT_UNWRAP(typeSpecifier, TemplatedIdent(), ErrorCode::ExpectedType);
        EXPECT_OPT(Tok::Comma);

        const ast::Member* member = nullptr;
        EXPECT_UNWRAP(member,
                      builder_->CreateMember(memberIdent.loc, memberIdent,
                                             typeSpecifier, attributes),
                      ErrorCode::TypeError);
        members.push_back(member);
    }
    if(members.empty()) {
        return Unexpected(ErrorCode::EmptyStruct);
    }
    return builder_->CreateStruct(structIdent.loc, structIdent, members);
}

//==========================================================================//

std::unexpected<ErrorCode> Parser::Unexpected(ErrorCode code,
                                              const std::string& msg) {
    builder_->ReportError(token_.loc, code, msg);
    return std::unexpected(code);
}

std::unexpected<ErrorCode> Parser::Unexpected(ErrorCode code) {
    return Unexpected(code, std::string(ErrorCodeDefaultMsg(code)));
}

std::unexpected<ErrorCode> Parser::Unexpected(Token::Kind kind) {
    auto msg = std::format("expected a {}", TokenToStringDiag(kind));
    builder_->ReportError(token_.loc, ErrorCode::UnexpectedToken, msg);
    return std::unexpected(ErrorCode::UnexpectedToken);
}

std::unexpected<ErrorCode> Parser::Unmatched() {
    return std::unexpected(ErrorCode::Unmatched);
}

bool Parser::Peek(Token::Kind kind) {
    return token_.kind == kind;
}

bool Parser::PeekKeyword(std::string_view name) {
    return Peek(Tok::Keyword) && Peek().Source() == name;
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