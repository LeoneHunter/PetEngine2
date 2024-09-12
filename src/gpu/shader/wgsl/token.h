#pragma once
#include "common.h"
#include <variant>

#if defined(EOF)
#undef EOF
#endif

namespace wgsl {

class Token {
public:
    enum class Kind {
        EOF,
        Invalid,
        Ident,
        Keyword,
        Reserved,
        Builtin,
        // Literal
        LitInt,
        LitUint,
        LitFloat,
        LitHalf,
        LitAbstrInt,
        LitAbstrFloat,
        // Operators
        Negation,          // !
        And,               // &
        AndAnd,            // &&
        Arrow,             // ->
        Div,               // /
        RightShift,        // >>
        Equal,             // =
        EqualEqual,        // ==
        GreaterThan,       // >
        LessThan,          // <
        GreaterThanEqual,  // >=
        LessThanEqual,     // <=
        LeftShift,         // <<
        Mod,               // %
        Minus,             // -
        MinusMinus,        // --
        NotEqual,          // !=
        Period,            // .
        Plus,              // +
        PlusPlus,          // ++
        Or,                // |
        OrOr,              // ||
        Mul,               // *
        Tilde,             // ~
        Xor,               // ^
        PlusEqual,         // +=
        MinusEqual,        // -=
        MulEqual,          // *=
        DivEqual,          // /=
        AndEqual,          // &=
        OrEqual,           // |=
        ShiftRightEqual,   // >>=
        ShiftLeftEqual,    // <<=
        // Control
        Attr,              // @
        OpenParen,         // (
        CloseParen,        // )
        Semicolon,         // ;
        Underscore,        // _
        OpenBracket,       // [
        CloseBracket,      // ]
        OpenBrace,         // {
        CloseBrace,        // }
        Colon,             // :
        Comma,             // ,
    };

    Kind kind = Kind::Invalid;
    SourceLoc loc;
    std::variant<int64_t, double, std::string_view> value;

public:
    static Token EOF(SourceLoc loc) { return {Kind::EOF, loc}; }
    static Token Invalid(SourceLoc loc) { return {Kind::Invalid, loc}; }

    constexpr Token() = default;

    constexpr Token(Kind kind, SourceLoc loc)
        : kind(kind), value(int64_t()), loc(loc) {}

    constexpr Token(Kind kind, SourceLoc loc, const char* start, const char* end)
        : kind(kind), value(std::string_view(start, end)), loc(loc) {}

    constexpr Token(Kind kind, SourceLoc loc, const char* start, uint32_t size)
        : kind(kind), value(std::string_view(start, size)), loc(loc) {}

    constexpr Token(Kind kind, SourceLoc loc, std::string_view source)
        : kind(kind), value(source), loc(loc) {}

    constexpr Token(Kind kind, SourceLoc loc, double value) 
        : kind(kind), value(value), loc(loc) {}

    constexpr Token(Kind kind, SourceLoc loc, int64_t value) 
        : kind(kind), value(value), loc(loc) {}

    constexpr auto operator<=>(const Token&) const = default;
    constexpr explicit operator bool() const { return kind != Kind::EOF; }

    constexpr Kind GetKind() const { return kind; }

    constexpr bool IsOperator() const { 
        return kind >= Kind::Negation && kind <= Kind::ShiftLeftEqual;
    }

    constexpr bool IsControl() const { 
        return kind >= Kind::Attr && kind <= Kind::Comma;
    }

    constexpr bool IsLiteral() const { 
        return kind >= Kind::LitInt && kind <= Kind::LitAbstrFloat;
    }

public:
    int64_t GetInt() {
        if (std::holds_alternative<int64_t>(value)) {
            return std::get<int64_t>(value);
        }
        return 0;
    }

    double GetDouble() {
        if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        }
        return 0.0;
    }

    const std::string_view Source() const {
        if (std::holds_alternative<std::string_view>(value)) {
            return std::get<std::string_view>(value);
        }
        return "";
    }

    template<class... T>
    constexpr bool Match(T&&... val) const { return (MatchImpl(val) || ...); }

private:
    constexpr bool MatchImpl(std::string_view val) const {
        return Source() == val;
    }
};

constexpr std::string_view to_string(Token::Kind kind) {
    switch (kind) {
        case Token::Kind::EOF: return "EOF";
        case Token::Kind::Invalid: return "Invalid";
        case Token::Kind::LitInt: return "LitInt";
        case Token::Kind::LitUint: return "LitUint";
        case Token::Kind::LitFloat: return "LitFloat";
        case Token::Kind::LitHalf: return "LitHalf";
        case Token::Kind::LitAbstrInt: return "LitAbstrInt";
        case Token::Kind::LitAbstrFloat: return "LitAbstrFloat";
        case Token::Kind::Ident: return "Ident";
        case Token::Kind::Keyword: return "Keyword";
        case Token::Kind::Reserved: return "Reserved";
        case Token::Kind::Negation: return "Negation";
        case Token::Kind::And: return "And";
        case Token::Kind::AndAnd: return "AndAnd";
        case Token::Kind::Arrow: return "Arrow";
        case Token::Kind::Div: return "Div";
        case Token::Kind::RightShift: return "RightShift";
        case Token::Kind::OpenBracket: return "OpenBracket";
        case Token::Kind::CloseBracket: return "CloseBracket";
        case Token::Kind::OpenBrace: return "OpenBrace";
        case Token::Kind::CloseBrace: return "CloseBrace";
        case Token::Kind::Colon: return "Colon";
        case Token::Kind::Comma: return "Comma";
        case Token::Kind::Equal: return "Equal";
        case Token::Kind::EqualEqual: return "EqualEqual";
        case Token::Kind::GreaterThan: return "GreaterThan";
        case Token::Kind::LessThan: return "LessThan";
        case Token::Kind::GreaterThanEqual: return "GreaterThanEqual";
        case Token::Kind::Attr: return "Attr";
        case Token::Kind::LessThanEqual: return "LessThanEqual";
        case Token::Kind::LeftShift: return "LeftShift";
        case Token::Kind::Mod: return "Mod";
        case Token::Kind::Minus: return "Minus";
        case Token::Kind::MinusMinus: return "MinusMinus";
        case Token::Kind::NotEqual: return "NotEqual";
        case Token::Kind::Period: return "Period";
        case Token::Kind::Plus: return "Plus";
        case Token::Kind::PlusPlus: return "PlusPlus";
        case Token::Kind::Or: return "Or";
        case Token::Kind::OrOr: return "OrOr";
        case Token::Kind::OpenParen: return "OpenParen";
        case Token::Kind::CloseParen: return "CloseParen";
        case Token::Kind::Semicolon: return "Semicolon";
        case Token::Kind::Mul: return "Mul";
        case Token::Kind::Tilde: return "Tilde";
        case Token::Kind::Underscore: return "Underscore";
        case Token::Kind::Xor: return "Xor";
        case Token::Kind::PlusEqual: return "PlusEqual";
        case Token::Kind::MinusEqual: return "MinusEqual";
        case Token::Kind::MulEqual: return "MulEqual";
        case Token::Kind::DivEqual: return "DivEqual";
        case Token::Kind::AndEqual: return "AndEqual";
        case Token::Kind::OrEqual: return "OrEqual";
        case Token::Kind::ShiftRightEqual: return "ShiftRightEqual";
        case Token::Kind::ShiftLeftEqual: return "ShiftLeftEqual";
        default: return "";
    }
}

// Token names for diagnostics
constexpr std::string TokenToStringDiag(Token::Kind kind) {
    std::string s = [&] {
        switch (kind) {
            case Token::Kind::EOF: return "end of file";
            case Token::Kind::Invalid: return "invalid";
            case Token::Kind::LitInt: return "int literal";
            case Token::Kind::LitUint: return "uint literal";
            case Token::Kind::LitFloat: return "float literal";
            case Token::Kind::LitHalf: return "half literal";
            case Token::Kind::LitAbstrInt: return "int literal";
            case Token::Kind::LitAbstrFloat: return "float literal";
            case Token::Kind::Ident: return "identifier";
            case Token::Kind::Keyword: return "keyword";
            case Token::Kind::Reserved: return "reserved";
            case Token::Kind::Negation: return "!";
            case Token::Kind::And: return "&";
            case Token::Kind::AndAnd: return "&&";
            case Token::Kind::Arrow: return "->";
            case Token::Kind::Div: return "/";
            case Token::Kind::RightShift: return ">>";
            case Token::Kind::OpenBracket: return "[";
            case Token::Kind::CloseBracket: return "]";
            case Token::Kind::OpenBrace: return "{";
            case Token::Kind::CloseBrace: return "}";
            case Token::Kind::Colon: return ":";
            case Token::Kind::Comma: return ",";
            case Token::Kind::Equal: return "=";
            case Token::Kind::EqualEqual: return "==";
            case Token::Kind::GreaterThan: return ">";
            case Token::Kind::LessThan: return "<";
            case Token::Kind::GreaterThanEqual: return ">=";
            case Token::Kind::Attr: return "@";
            case Token::Kind::LessThanEqual: return "<=";
            case Token::Kind::LeftShift: return "<<";
            case Token::Kind::Mod: return "%";
            case Token::Kind::Minus: return "-";
            case Token::Kind::MinusMinus: return "--";
            case Token::Kind::NotEqual: return "!=";
            case Token::Kind::Period: return ".";
            case Token::Kind::Plus: return "+";
            case Token::Kind::PlusPlus: return "++";
            case Token::Kind::Or: return "|";
            case Token::Kind::OrOr: return "||";
            case Token::Kind::OpenParen: return "(";
            case Token::Kind::CloseParen: return ")";
            case Token::Kind::Semicolon: return ";";
            case Token::Kind::Mul: return "*";
            case Token::Kind::Tilde: return "~";
            case Token::Kind::Underscore: return "_";
            case Token::Kind::Xor: return "^";
            case Token::Kind::PlusEqual: return "+=";
            case Token::Kind::MinusEqual: return "-=";
            case Token::Kind::MulEqual: return "*=";
            case Token::Kind::DivEqual: return "/=";
            case Token::Kind::AndEqual: return "&=";
            case Token::Kind::OrEqual: return "|=";
            case Token::Kind::ShiftRightEqual: return ">>=";
            case Token::Kind::ShiftLeftEqual: return "<<=";
            default: return "";
        }
    }();
    return std::string("'") + s + "'";
}

} // namespace wgsl