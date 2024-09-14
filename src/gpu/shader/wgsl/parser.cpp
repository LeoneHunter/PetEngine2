#include "parser.h"
#include "program.h"
#include "program_builder.h"

#include <functional>

namespace wgsl {

#define EXPECT_TOKEN(TOKEN)           \
    do {                              \
        if (Peek(TOKEN)) {            \
            Advance();                \
        } else {                      \
            return Unexpected(TOKEN); \
        }                             \
    } while (false)

#define MATCH_TOKEN(TOKEN)      \
    do {                        \
        if (Peek(TOKEN)) {      \
            Advance();          \
        } else {                \
            return Unmatched(); \
        }                       \
    } while (false)

#define MAYBE_TOKEN(TOKEN) \
    do {                   \
        if (Peek(TOKEN)) { \
            Advance();     \
        }                  \
    } while (false)

// Expect a result of expr to have a value
// Else forwards errors up
#define VALUE_ELSE_RET(NODE, EXPR)          \
    do {                                    \
        auto res = EXPR;                    \
        if (res) {                          \
            NODE = *res;                    \
        } else {                            \
            /* Forward error */             \
            return Unexpected(res.error()); \
        }                                   \
    } while (false)

// Try unwrap the result of expr, return if ok or error
#define IF_VALUE_RET(EXPR, ...)                           \
    do {                                                  \
        auto res = EXPR;                                  \
        if (res) {                                        \
            return *res;                                  \
        } else if (res.error() != ErrorCode::Unmatched) { \
            return Unexpected(res.error());               \
        }                                                 \
    } while (false)

// Try to unwrap the resultof expr, else forward or report an error
#define VALUE_ELSE_RET_ERR(NODE, EXPR, ...)               \
    do {                                                  \
        auto res = EXPR;                                  \
        if (res) {                                        \
            NODE = *res;                                  \
        } else if (res.error() != ErrorCode::Unmatched) { \
            /* Forward error */                           \
            return Unexpected(res.error());               \
        } else {                                          \
            return Unexpected(__VA_ARGS__);               \
        }                                                 \
    } while (false)

#define MAYBE_VALUE(NODE, EXPR)                           \
    do {                                                  \
        auto res = EXPR;                                  \
        if (res) {                                        \
            NODE = *res;                                  \
        } else if (res.error() != ErrorCode::Unmatched) { \
            return Unexpected(res.error());               \
        }                                                 \
    } while (false)

// If result of the expr is void (...)
#define IF_VOID(EXPR, ...)                                \
    {                                                  \
        auto res = EXPR;                                  \
        if (res) {                                        \
            __VA_ARGS__;                                  \
        } else if (res.error() != ErrorCode::Unmatched) { \
            return Unexpected(res.error());               \
        }                                                 \
    } void()

// Expect not an error
#define VOID_ELSE_RET(EXPR)                 \
    do {                                    \
        auto res = EXPR;                    \
        if (!res) {                         \
            return Unexpected(res.error()); \
        }                                   \
    } while (false)

#define VOID_ELSE_RET_ERR(EXPR, ...)                      \
    do {                                                  \
        auto res = EXPR;                                  \
        if (res) {                                        \
        } else if (res.error() != ErrorCode::Unmatched) { \
            /* Forward error */                           \
            return Unexpected(res.error());               \
        } else {                                          \
            return Unexpected(__VA_ARGS__);               \
        }                                                 \
    } while (false)


// Check the condition, if false returns
#define EXPECT_COND(COND, ...)              \
    do {                                    \
        if (!(COND)) {                      \
            return Unexpected(__VA_ARGS__); \
        }                                   \
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
        if (GlobalDecl()) {
            continue;
        }
        // Improve errors
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

// ';' -> empty decl, return true
// attribute* 'fn' -> function
// attribute* 'var' -> var
// attribute* 'override' -> override value
// 'const' -> const value
// 'alias' -> type alias
// 'struct' -> struct
ExpectedVoid Parser::GlobalDecl() {
    if (Expect(Tok::Semicolon)) {
        return Void();
    }
    // Try parse attributes
    ast::AttributeList attributes;
    while (Peek(Tok::Attr)) {
        const ast::Attribute* attr = nullptr;
        VALUE_ELSE_RET(attr, Attribute());
        attributes.push_back(attr);
    }
    // Declarations with attributes
    // fn, var, override
    if (Peek(Keyword::Fn)) {
        VOID_ELSE_RET(FunctionDecl(attributes));
        return Void();
    }
    if (Peek(Keyword::Var)) {
        VOID_ELSE_RET(VariableDecl(attributes));
        EXPECT_TOKEN(Tok::Semicolon);
        return Void();
    }
    if (Peek(Keyword::Override)) {
        // ast::OverrideVar* decl = nullptr;
        // TRY_UNWRAP(decl, VarDecl(attributes));
        // builder_->PushGlobalDecl(decl);
        // TODO: Implement
        return Unexpected(ErrorCode::Unimplemented);
    }
    EXPECT_COND(attributes.empty(), ErrorCode::UnexpectedToken,
                "expected a 'fn', 'var' or 'override'");
    if (Peek(Keyword::Const)) {
        VOID_ELSE_RET(ConstValueDecl());
        EXPECT_TOKEN(Tok::Semicolon);
        return Void();
    }
    if (Peek(Keyword::Alias)) {
        // TODO: Implement
        return Unexpected(ErrorCode::Unimplemented);
    }
    if (Peek(Keyword::Struct)) {
        VOID_ELSE_RET(StructDecl());
        return Void();
    }
    return Unmatched();
}

// function_decl:
//   attribute* 'fn' ident '('
//        ( attribute* ident ':' type_specifier ( ',' param )* ',' ? )?
//   ')' ( '->' attribute* type_specifier )? attribute*
//   '{' statement * '}'
ExpectedVoid Parser::FunctionDecl(ast::AttributeList& attributes) {
    MATCH_TOKEN(Keyword::Fn);
    EXPECT_TOKEN(Tok::Ident);
    const auto ident = Ident{GetLastToken().loc, GetLastToken().Source()};
    // Parameters
    EXPECT_TOKEN(Tok::OpenParen);
    ast::ParameterList parameters;
    while (!Expect(Tok::CloseParen)) {
        const ast::Parameter* param = nullptr;
        VALUE_ELSE_RET_ERR(param, ParameterDecl(), ErrorCode::ExpectedIdent,
                           "expected a parameter declaration");
        parameters.push_back(param);
        MAYBE_TOKEN(Tok::Comma);
    }
    // Return type
    ast::AttributeList returnAttributes;
    Ident returnType;
    if (Expect(Tok::Arrow)) {
        while (Peek(Tok::Attr)) {
            const ast::Attribute* attr = nullptr;
            VALUE_ELSE_RET(attr, Attribute());
            returnAttributes.push_back(attr);
        }
        VALUE_ELSE_RET_ERR(returnType, TemplatedIdent(),
                           ErrorCode::ExpectedType,
                           "expected a return type declaration");
    }
    builder_->DeclareFunction(ident.loc, ident, attributes, parameters,
                              returnType, returnAttributes);
    // Parse scope
    EXPECT_TOKEN(Tok::OpenBrace);
    while (!Expect(Tok::CloseBrace)) {
        IF_VOID(Statement(), continue);
        // TODO: Parse attributes
        ast::AttributeList attrList;
        IF_VOID(VariableDecl(attrList), continue);
        return Unexpected(ErrorCode::ExpectedStatement);
    }
    builder_->DeclareFuncEnd(GetLastToken().loc);
    return Void();
}

// param :
//   attribute * ident ':' type_specifier
Expected<const ast::Parameter*> Parser::ParameterDecl() {
    ast::AttributeList attributes;
    while (Peek(Tok::Attr)) {
        const ast::Attribute* attr = nullptr;
        VALUE_ELSE_RET(attr, Attribute());
        attributes.push_back(attr);
    }
    EXPECT_TOKEN(Tok::Ident);
    const auto ident = Ident{GetLastToken().loc, GetLastToken().Source()};
    EXPECT_TOKEN(Tok::Colon);
    Ident typeSpecifier;
    VALUE_ELSE_RET_ERR(typeSpecifier, TemplatedIdent(),
                       ErrorCode::ExpectedType);
    return builder_->CreateParameter(ident.loc, ident, typeSpecifier,
                                     attributes);
}

// Summary:
//   | ';'
//   | for, if, switch, loop, while
//   | { statement }
//   | fn_call
//   | var, const, let
//   | =, ++, --, _
//   | break, continue
//   | const_assert
//   | discard
//   | return
ExpectedVoid Parser::Statement() {
    while (Peek(Tok::Semicolon)) {
        Advance();
    }
    //  'return' expression? ';'
    if (Expect(Keyword::Return)) {
        const SourceLoc loc = GetLastToken().loc;
        const ast::Expression* expr = nullptr;
        MAYBE_VALUE(expr, Expression(false));
        VOID_ELSE_RET(builder_->DeclareReturnStatement(loc, expr));
        EXPECT_TOKEN(Tok::Semicolon);
        return Void();
    }
    return Unexpected(ErrorCode::ExpectedStatement);
}

// compound_statement:
//   attribute* '{' statement* '}'
ExpectedVoid Parser::CompoundStatement() {
    MATCH_TOKEN(Tok::OpenParen);
    ProgramList<const ast::Statement*> statements;
    while (!Expect(Tok::CloseParen)) {
        // ';'
        if (Expect(Tok::Semicolon)) {
            continue;
        }
        // | if_statement
        // | switch_statement
        // | loop_statement
        // | for_statement
        // | while_statement
        // | func_call_statement ';'
        // | variable_or_value_statement ';'
        // | break_statement ';'
        // | continue_statement ';'
        // | 'discard' ';'
        // | variable_updating_statement ';'
        // | compound_statement
        // | const_assert_statement ';'
    }
    return Unexpected(ErrorCode::UnexpectedToken);
}


// global_decl:
//   'struct' ident '{'
//      attribute* ident ':' type_specifier
//      ( ',' attribute* ident ':' type_specifier )* ',' ?
//   '}'
ExpectedVoid Parser::StructDecl() {
    MATCH_TOKEN(Keyword::Struct);
    EXPECT_TOKEN(Tok::Ident);
    const Token tok = GetLastToken();
    const Ident structIdent = Ident{tok.loc, tok.Source()};

    EXPECT_TOKEN(Tok::OpenBrace);
    ast::MemberList members;

    while (!Expect(Tok::CloseBrace)) {
        ast::AttributeList attributes;
        while (Peek(Tok::Attr)) {
            const ast::Attribute* attr = nullptr;
            VALUE_ELSE_RET(attr, Attribute());
            attributes.push_back(attr);
        }
        EXPECT_TOKEN(Tok::Ident);
        const Token memberTok = GetLastToken();
        const Ident memberIdent = Ident{memberTok.loc, memberTok.Source()};
        EXPECT_TOKEN(Tok::Colon);
        Ident typeSpecifier;
        VALUE_ELSE_RET_ERR(typeSpecifier, TemplatedIdent(),
                           ErrorCode::ExpectedType);
        MAYBE_TOKEN(Tok::Comma);

        const ast::Member* member = nullptr;
        VALUE_ELSE_RET(member,
                       builder_->CreateMember(memberIdent.loc, memberIdent,
                                              typeSpecifier, attributes));
        members.push_back(member);
    }
    if (members.empty()) {
        return Unexpected(ErrorCode::EmptyStruct);
    }
    builder_->DeclareStruct(structIdent.loc, structIdent, members);
    return Ok();
}

// 'const' ident ( ':' type_specifier )? '=' expression
ExpectedVoid Parser::ConstValueDecl() {
    // 'const'
    if (!Peek(Keyword::Const)) {
        return Unmatched();
    }
    SourceLoc loc = Advance().loc;
    // ident
    EXPECT_TOKEN(Tok::Ident);
    Token ident = GetLastToken();
    // type_specifier
    SourceLoc typeLoc = Peek().loc;
    std::optional<Ident> typeSpecifier;
    if (Expect(Tok::Colon)) {
        VALUE_ELSE_RET_ERR(typeSpecifier, TemplatedIdent(),
                           ErrorCode::ExpectedType);
    }
    // '=' expression
    EXPECT_TOKEN(Tok::Equal);
    const ast::Expression* initializer = nullptr;
    VALUE_ELSE_RET_ERR(initializer, Expression(false), ErrorCode::ExpectedExpr);
    builder_->DeclareConst(loc, Ident(ident.loc, ident.Source()), typeSpecifier,
                           initializer);
    return Ok();
}

// 'override'
ExpectedVoid Parser::OverrideValueDecl(ast::AttributeList& attributes) {
    if (!Peek(Keyword::Override)) {
        return Unmatched();
    }
    return Unexpected(ErrorCode::Unimplemented,
                      "'override' variables not implemented");
}

// 'var' template_list? ident ( : type)? ( '=' expression )? ';'
ExpectedVoid Parser::VariableDecl(ast::AttributeList& attributes) {
    // 'var'
    if (!Peek(Keyword::Var)) {
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
        EXPECT_TOKEN(Tok::GreaterThan);
    }
    // Ident
    Ident ident;
    VALUE_ELSE_RET_ERR(ident, TemplatedIdent(), ErrorCode::ExpectedIdent);
    // ':'
    std::optional<Ident> typeSpecifier;
    if (Expect(Tok::Colon)) {
        VALUE_ELSE_RET_ERR(typeSpecifier, TemplatedIdent(),
                           ErrorCode::ExpectedType);
    }
    // Initializer: '=' expression
    const ast::Expression* initializer = nullptr;
    if (Expect(Tok::Equal)) {
        VALUE_ELSE_RET_ERR(initializer, Expression(false),
                           ErrorCode::ExpectedExpr);
    }
    builder_->DeclareVariable(loc, ident, addrSpace, accessMode, typeSpecifier,
                              attributes, initializer);
    return Ok();
}

// ident ( '<' expression ( ',' expression )* ','? '>' )?
Expected<Ident> Parser::TemplatedIdent() {
    MATCH_TOKEN(Tok::Ident);
    Token ident = GetLastToken();
    ExpressionList templ;
    // Template parameter list: '<'
    if (Expect(Tok::LessThan)) {
        while (!Expect(Tok::GreaterThan)) {
            const ast::Expression* expr = nullptr;
            VALUE_ELSE_RET_ERR(expr, Expression(true), ErrorCode::ExpectedExpr);
            templ.push_back(expr);
            MAYBE_TOKEN(Tok::Comma);
        }
    }
    return Ident{ident.loc, ident.Source(), templ};
}

// unary_expression | relational_expr ...
Expected<const ast::Expression*> Parser::Expression(bool inTemplate) {
    const ast::Expression* unary = nullptr;
    VALUE_ELSE_RET(unary, UnaryExpr());
    // binary operator
    using Op = ast::OpCode;
    std::optional<Op> op;

    Token tok = Peek();
    switch (tok.kind) {
        // Arithmetic
        case Tok::Mul: op = Op::Mul; break;
        case Tok::Div: op = Op::Div; break;
        case Tok::Plus: op = Op::Add; break;
        case Tok::Minus: op = Op::Sub; break;
        case Tok::Mod: op = Op::Mod; break;
        // Relation
        case Tok::LessThan: op = Op::Less; break;
        case Tok::LessThanEqual: op = Op::LessEqual; break;
        case Tok::GreaterThanEqual: op = Op::GreaterEqual; break;
        case Tok::Equal: op = Op::Equal; break;
        case Tok::NotEqual: op = Op::NotEqual; break;
        // Logic
        case Tok::AndAnd: op = Op::LogAnd; break;
        case Tok::OrOr: op = Op::LogOr; break;
        // Bitwise
        case Tok::And: op = Op::BitAnd; break;
        case Tok::Or: op = Op::BitOr; break;
        case Tok::Xor: op = Op::BitXor; break;
        case Tok::LeftShift: op = Op::BitLsh; break;
        case Tok::RightShift: op = Op::BitRsh; break;
        case Tok::GreaterThan:
            if (!inTemplate) {
                op = Op::Greater;
            }
    }
    if (!op) {
        return unary;
    }
    Advance();
    // Expect rhs expression
    const ast::Expression* lhs = unary;
    const ast::Expression* rhs = nullptr;
    VALUE_ELSE_RET_ERR(rhs, Expression(inTemplate), ErrorCode::ExpectedExpr);
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
Expected<const ast::Expression*> Parser::UnaryExpr() {
    // primary_expression
    const ast::Expression* primary = nullptr;
    MAYBE_VALUE(primary, PrimaryExpr());
    if (primary) {
        auto res2 = ComponentSwizzleExpr(primary);
        if (res2) {
            return *res2;
        }
        if (res2.error() == ErrorCode::Unmatched) {
            return primary;
        }
    }
    // Unary operator
    Token opToken = Peek();
    using Op = ast::OpCode;
    std::optional<Op> op;

    switch (opToken.kind) {
        case Tok::Negation: op = Op::LogNot; break;
        case Tok::Tilde: op = Op::BitNot; break;
        case Tok::Minus: op = Op::Negation; break;
        case Tok::And:
        case Tok::Mul:
            return Unexpected(ErrorCode::Unimplemented,
                              "pointers are unsupported");
    }
    if (op) {
        Advance();
        const ast::Expression* rhs = nullptr;
        VALUE_ELSE_RET_ERR(rhs, UnaryExpr(), ErrorCode::ExpectedExpr);
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
Expected<const ast::Expression*> Parser::PrimaryExpr() {
    // literal
    IF_VALUE_RET(IntLiteralExpr());
    IF_VALUE_RET(FloatLiteralExpr());
    IF_VALUE_RET(BoolLiteralExpr());
    // ident template_list
    IF_VALUE_RET(IdentExpression());
    // '(' expression ')'
    if (Expect(Tok::OpenParen)) {
        const ast::Expression* expr = nullptr;
        VALUE_ELSE_RET_ERR(expr, Expression(false), ErrorCode::ExpectedExpr);
        EXPECT_TOKEN(Tok::CloseParen);
        return expr;
    }
    return Unmatched();
}

// component_or_swizzle_specifier:
//   | '.' member_ident component_or_swizzle_specifier ?
//   | '.' swizzle_name component_or_swizzle_specifier ?
//   | '[' expression ']' component_or_swizzle_specifier ?
Expected<const ast::Expression*> wgsl::Parser::ComponentSwizzleExpr(
    const ast::Expression* lhs) {
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
Expected<const ast::Expression*> Parser::IdentExpression() {
    MATCH_TOKEN(Tok::Ident);
    Token tok = GetLastToken();
    Ident ident;
    ident.loc = tok.loc;
    ident.name = tok.Source();
    // Template parameter list: '<'
    if (Expect(Tok::LessThan)) {
        while (!Expect(Tok::GreaterThan)) {
            const ast::Expression* expr = nullptr;
            VALUE_ELSE_RET_ERR(expr, Expression(true), ErrorCode::ExpectedExpr);
            ident.templateList.push_back(expr);
            MAYBE_TOKEN(Tok::Comma);
        }
    }
    // function call
    if (Expect(Tok::OpenParen)) {
        ExpressionList args;
        while (!Expect(Tok::CloseParen)) {
            const ast::Expression* expr = nullptr;
            VALUE_ELSE_RET_ERR(expr, Expression(false),
                               ErrorCode::ExpectedExpr);
            args.push_back(expr);
            MAYBE_TOKEN(Tok::Comma);
        }
        return builder_->CreateFnCallExpr(ident, args);
    }
    return builder_->CreateIdentExpr(ident);
}

Expected<const ast::IntLiteralExpression*> Parser::IntLiteralExpr() {
    if (PeekAny(Tok::LitAbstrInt, Tok::LitInt, Tok::LitUint)) {
        Token tok = Advance();
        auto type = ast::ScalarKind::Int;
        if (tok.kind == Tok::LitInt) {
            type = ast::ScalarKind::I32;
        } else if (tok.kind == Tok::LitUint) {
            type = ast::ScalarKind::U32;
        }
        return builder_->CreateIntLiteralExpr(tok.loc, tok.GetInt(), type);
    }
    return Unmatched();
}

Expected<const ast::FloatLiteralExpression*> Parser::FloatLiteralExpr() {
    if (PeekAny(Tok::LitAbstrFloat, Tok::LitFloat, Tok::LitHalf)) {
        Token tok = Advance();
        auto type = ast::ScalarKind::Float;
        if (tok.kind == Tok::LitFloat) {
            type = ast::ScalarKind::F32;
        } else if (tok.kind == Tok::LitHalf) {
            type = ast::ScalarKind::F16;
        }
        return builder_->CreateFloatLiteralExpr(tok.loc, tok.GetDouble(), type);
    }
    return Unmatched();
}

Expected<const ast::BoolLiteralExpression*> Parser::BoolLiteralExpr() {
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
Expected<const ast::Attribute*> Parser::Attribute() {
    MATCH_TOKEN(Tok::Attr);
    EXPECT_TOKEN(Tok::Ident);
    const Token ident = GetLastToken();
    AttributeName attr;
    if (auto res = AttributeFromString(ident.Source())) {
        attr = *res;
    } else {
        return Unexpected(ErrorCode::InvalidAttribute);
    }
    EXPECT_COND(attr != AttributeName::Const, ErrorCode::InvalidAttribute);
    // TODO: implement
    EXPECT_COND(attr != AttributeName::Diagnostic, ErrorCode::Unimplemented);
    EXPECT_COND(attr != AttributeName::Interpolate, ErrorCode::Unimplemented);
    // Expect a single param inside parens
    switch (attr) {
        case AttributeName::MustUse:
        case AttributeName::Invariant:
        case AttributeName::Vertex:
        case AttributeName::Fragment:
        case AttributeName::Compute: {
            return builder_->CreateAttribute(ident.loc, attr, nullptr);
        }
        case AttributeName::Align:
        case AttributeName::Binding:
        case AttributeName::BlendSrc:
        case AttributeName::Group:
        case AttributeName::ID:
        case AttributeName::Location:
        case AttributeName::Size: {
            EXPECT_TOKEN(Tok::OpenParen);
            const ast::Expression* expr = nullptr;
            VALUE_ELSE_RET_ERR(expr, Expression(false),
                               ErrorCode::ExpectedExpr);
            MAYBE_TOKEN(Tok::Comma);
            EXPECT_TOKEN(Tok::CloseParen);
            return builder_->CreateAttribute(ident.loc, attr, expr);
        }
        case AttributeName::WorkgroupSize: {
            EXPECT_TOKEN(Tok::OpenParen);
            const ast::Expression* xyz[] = {nullptr};
            for (uint32_t i = 0; i < 3; ++i) {
                if (Peek(Tok::CloseParen)) {
                    break;
                }
                VALUE_ELSE_RET_ERR(xyz[i], Expression(false),
                                   ErrorCode::ExpectedExpr);
                MAYBE_TOKEN(Tok::Comma);
            }
            return builder_->CreateWorkGroupAttr(ident.loc, xyz[0], xyz[1],
                                                 xyz[2]);
        }
        case AttributeName::Builtin: {
            EXPECT_TOKEN(Tok::OpenParen);
            EXPECT_TOKEN(Tok::Builtin);
            const Token tok = GetLastToken();
            const Builtin value = *BuiltinFromString(tok.Source());
            MAYBE_TOKEN(Tok::Comma);
            EXPECT_TOKEN(Tok::CloseParen);
            return builder_->CreateBuiltinAttribute(ident.loc, value);
        }
    }
    return Unexpected(ErrorCode::InvalidAttribute,
                      "'{}' is not a valid attribute name", ident.Source());
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

bool Parser::Peek(Keyword Keyword) {
    return Peek(Tok::Keyword) && Peek().Source() == to_string(Keyword);
}

bool Parser::Expect(Token::Kind kind) {
    if (Peek(kind)) {
        Advance();
        return true;
    }
    return false;
}

bool Parser::Expect(Keyword keyword) {
    if (Peek(keyword)) {
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