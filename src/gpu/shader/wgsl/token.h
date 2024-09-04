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
        LitInt,
        LitUint,
        LitFloat,
        LitHalf,
        LitAbstrInt,
        LitAbstrFloat,
        LitBool,
        Ident,
        Keyword,
        Negation,          // !
        And,               // &
        AndAnd,            // &&
        Arrow,             // ->
        Div,               // /
        RightShift,        // >>
        OpenBracket,       // [
        CloseBracket,      // ]
        OpenBrace,         // {
        CloseBrace,        // }
        Colon,             // :
        Comma,             // ,
        Equal,             // =
        EqualEqual,        // ==
        GreaterThan,       // >
        LessThan,          // <
        GreaterThanEqual,  // >=
        Attr,              // @
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
        OpenParen,         // (
        CloseParen,        // )
        Semicolon,         // ;
        Mul,               // *
        Tilde,             // ~
        Underscore,        // _
        Xor,               // ^
        PlusEqual,         // +=
        MinusEqual,        // -=
        MulEqual,          // *=
        DivEqual,          // /=
        AndEqual,          // &=
        OrEqual,           // |=
        ShiftRightEqual,   // >>=
        ShiftLeftEqual,    // <<=
    };

    Kind kind = Kind::Invalid;
    Location loc;
    std::variant<int64_t, double, std::string_view> value;

public:
    static Token EOF(Location loc) { return {Kind::EOF, loc}; }
    static Token Invalid(Location loc) { return {Kind::Invalid, loc}; }

    constexpr Token() = default;

    constexpr Token(Kind kind, Location loc)
        : kind(kind), value(int64_t()), loc(loc) {}

    constexpr Token(Kind kind, Location loc, const char* start, const char* end)
        : kind(kind), value(std::string_view(start, end)), loc(loc) {}

    constexpr Token(Kind kind, Location loc, const char* start, uint32_t size)
        : kind(kind), value(std::string_view(start, size)), loc(loc) {}

    constexpr Token(Kind kind, Location loc, std::string_view span)
        : kind(kind), value(span), loc(loc) {}

    constexpr Token(Kind kind, Location loc, double value) 
        : kind(kind), value(value), loc(loc) {}

    constexpr Token(Kind kind, Location loc, int64_t value) 
        : kind(kind), value(value), loc(loc) {}

    constexpr auto operator<=>(const Token&) const = default;
    constexpr explicit operator bool() const { return kind != Kind::EOF; }

    constexpr Kind GetKind() const { return kind; }

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

    std::string_view GetString() {
        if (std::holds_alternative<std::string_view>(value)) {
            return std::get<std::string_view>(value);
        }
        return "";
    }

    template<class... T>
    constexpr bool Match(T&&... val) { return (MatchImpl(val) || ...); }

private:
    constexpr bool MatchImpl(std::string_view val) {
        return GetString() == val;
    }
};

constexpr std::string to_string(Token::Kind kind) {
    switch (kind) {
        case Token::Kind::EOF: return "EOF";
        case Token::Kind::Invalid: return "Invalid";
        case Token::Kind::LitInt: return "LitInt";
        case Token::Kind::LitUint: return "LitUint";
        case Token::Kind::LitFloat: return "LitFloat";
        case Token::Kind::LitHalf: return "LitHalf";
        case Token::Kind::LitAbstrInt: return "LitAbstrInt";
        case Token::Kind::LitAbstrFloat: return "LitAbstrFloat";
        case Token::Kind::LitBool: return "LitBool";
        case Token::Kind::Ident: return "Ident";
        case Token::Kind::Keyword: return "Keyword";
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

} // namespace wgsl