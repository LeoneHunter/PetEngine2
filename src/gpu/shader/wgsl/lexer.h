#pragma once
#include "token.h"
#include "util.h"
#include <array>

namespace wgsl {

// Text lexer
class Lexer {
public:
    constexpr Lexer(std::string_view code) {
        text_ = code;
        next_ = code.data();
        end_ = code.data() + code.size();
        lineStart_ = next_;
        loc_ = Location{0, 1};
    }

    std::vector<Token> ParseAll() {
        std::vector<Token> out;
        while (out.emplace_back(ParseNext()))
            ;
        return out;
    }

    bool ParseNext(Token& out) {
        out = ParseNext();
        return out.kind != Token::Kind::EOF;
    }

    Token ParseNext() {
        while (!IsEof() && !Match(CharKind::Digit, CharKind::Letter,
                                  CharKind::Punctuation)) {
            SkipBlankspace();
            SkipComment();
        }
        if (IsEof()) {
            return Token::EOF(Loc());
        }
        switch (GetKind()) {
            case CharKind::Digit: return ParseDigit();
            case CharKind::Letter: return ParseLetter();
            case CharKind::Punctuation: return ParsePunctuation();
        }
        return Token::Invalid(Loc());
    }

    // Get current line
    std::string_view GetLine() { return {lineStart_, GetLineEnd()}; }

private:
    // Character kind
    enum class CharKind {
        Digit,
        Letter,
        Punctuation,
        Space,
        LineBreak,
    };

    Token ParseDigit() {
        struct Flags {
            uint16_t prefixHex : 1;
            uint16_t floatType : 1;
            uint16_t suffixH : 1;
            uint16_t suffixF : 1;
            uint16_t intType : 1;
            uint16_t suffixI : 1;
            uint16_t suffixU : 1;
        };
        Flags flags{};
        const char* start = Pos();
        Location loc = Loc();
        // Scan for type identifying symbols: 0[xX], [.e+-]
        if (Match("0x", "0X")) {
            flags.prefixHex = true;
            flags.intType = true;
            Advance(2);
            start = Pos();
        }
        while (Match(CharKind::Digit, '.', 'e', '+', '-')) {
            if (Match('.', 'e')) {
                flags.floatType = true;
            }
            Advance();
        }
        // Check for user specified type [iufh]
        if (Match('h')) {
            flags.floatType = true;
            flags.suffixH = true;
            Advance();
        } else if (Match('f')) {
            flags.floatType = true;
            flags.suffixF = true;
            Advance();
        } else if (Match('u')) {
            flags.intType = true;
            flags.suffixU = true;
            Advance();
        } else if (Match('i')) {
            flags.intType = true;
            flags.suffixI = true;
            Advance();
        }
        // If no special symbols are found assume uint
        if (!flags.intType && !flags.floatType) {
            flags.intType = true;
        }
        // Try to parse
        if (flags.floatType) {
            Token token = Token::Invalid(loc);
            TryParseAsFloat(start, token);
            if (flags.suffixF) {
                token.kind = Token::Kind::LitFloat;
            } else if (flags.suffixH) {
                token.kind = Token::Kind::LitHalf;
            } else {
                token.kind = Token::Kind::LitAbstrFloat;
            }
            return token;
        }
        if (flags.intType) {
            Token token = Token::Invalid(loc);
            TryParseAsInt(start, token, flags.prefixHex);
            if (flags.suffixU) {
                token.kind = Token::Kind::LitUint;
            } else if (flags.suffixI) {
                token.kind = Token::Kind::LitInt;
            } else {
                token.kind = Token::Kind::LitAbstrInt;
            }
            return token;
        }
        return Token::Invalid(loc);
    }

    constexpr Token ParseLetter() {
        for (const std::string_view keyword : kKeywords) {
            if (Match(keyword)) {
                const auto out =
                    Token(Token::Kind::Keyword, Loc(), Pos(), keyword.size());
                Advance(keyword.size());
                return out;
            }
        }
        for (const std::string_view reserved : kReserved) {
            if (Match(reserved)) {
                const auto out = 
                    Token(Token::Kind::Reserved, Loc(), Pos(), reserved.size());
                Advance(reserved.size());
                return out;
            }
        }
        if (Match("true")) {
            const auto tok = Token(Token::Kind::LitBool, Loc(), 1LL);
            Advance(4);
            return tok;
        }
        if (Match("false")) {
            const auto tok = Token(Token::Kind::LitBool, Loc(), 0LL);
            Advance(5);
            return tok;
        }
        const char* start = next_;
        Location loc = Loc();
        while (Match(CharKind::Digit, CharKind::Letter, '_')) {
            Advance();
        }
        return Token(Token::Kind::Ident, loc, start, Pos());
    }

    constexpr Token ParsePunctuation() {
        switch (*Pos()) {
            case '&': {
                if (Match("&&")) {
                    return Op2(Token::Kind::AndAnd);
                }
                if (Match("&=")) {
                    return Op2(Token::Kind::AndEqual);
                }
                return Op1(Token::Kind::And);
            }
            case '-': {
                if (Match("->")) {
                    return Op2(Token::Kind::Arrow);
                }
                if (Match("--")) {
                    return Op2(Token::Kind::MinusMinus);
                }
                if (Match("-=")) {
                    return Op2(Token::Kind::MinusEqual);
                }
                return Op1(Token::Kind::Minus);
            }
            case '/': {
                if (Match("/=")) {
                    return Op2(Token::Kind::DivEqual);
                }
                return Op1(Token::Kind::Div);
            }
            case '!': {
                if (Match("!=")) {
                    return Op2(Token::Kind::NotEqual);
                }
                return Op1(Token::Kind::Negation);
            }
            case '=': {
                if (Match("==")) {
                    return Op2(Token::Kind::EqualEqual);
                }
                return Op1(Token::Kind::Equal);
            }
            case '|': {
                if (Match("||")) {
                    return Op2(Token::Kind::OrOr);
                }
                if (Match("|=")) {
                    return Op2(Token::Kind::OrEqual);
                }
                return Op1(Token::Kind::Or);
            }
            case '+': {
                if (Match("++")) {
                    return Op2(Token::Kind::PlusPlus);
                }
                if (Match("+=")) {
                    return Op2(Token::Kind::PlusEqual);
                }
                return Op1(Token::Kind::Plus);
            }
            case '>': {
                if (Match(">>")) {
                    return Op2(Token::Kind::RightShift);
                }
                if (Match(">=")) {
                    return Op2(Token::Kind::GreaterThanEqual);
                }
                if (Match(">>=")) {
                    return Op3(Token::Kind::ShiftRightEqual);
                }
                return Op1(Token::Kind::GreaterThan);
            }
            case '<': {
                if (Match("<<")) {
                    return Op2(Token::Kind::LeftShift);
                }
                if (Match("<=")) {
                    return Op2(Token::Kind::LessThanEqual);
                }
                if (Match("<<=")) {
                    return Op3(Token::Kind::ShiftLeftEqual);
                }
                return Op1(Token::Kind::LessThan);
            }
            case '*': {
                if (Match("*=")) {
                    return Op2(Token::Kind::MulEqual);
                }
                return Op1(Token::Kind::Mul);
            }
            case '[': {
                return Op1(Token::Kind::OpenBracket);
            }
            case ']': {
                return Op1(Token::Kind::CloseBracket);
            }
            case '(': {
                return Op1(Token::Kind::OpenParen);
            }
            case ')': {
                return Op1(Token::Kind::CloseParen);
            }
            case '{': {
                return Op1(Token::Kind::OpenBrace);
            }
            case '}': {
                return Op1(Token::Kind::CloseBrace);
            }
            case ':': {
                return Op1(Token::Kind::Colon);
            }
            case ';': {
                return Op1(Token::Kind::Semicolon);
            }
            case ',': {
                return Op1(Token::Kind::Comma);
            }
            case '.': {
                return Op1(Token::Kind::Period);
            }
            case '@': {
                return Op1(Token::Kind::Attr);
            }
            case '%': {
                return Op1(Token::Kind::Mod);
            }
            case '~': {
                return Op1(Token::Kind::Tilde);
            }
            case '_': {
                return Op1(Token::Kind::Underscore);
            }
            case '^': {
                return Op1(Token::Kind::Xor);
            }
        }
        return Token::Invalid(Loc());
    }

    constexpr Token Op3(Token::Kind kind) {
        auto lex = Token(kind, Loc(), Pos(), 3);
        Advance(3);
        return lex;
    }

    constexpr Token Op2(Token::Kind kind) {
        auto lex = Token(kind, Loc(), Pos(), 2);
        Advance(2);
        return lex;
    }

    constexpr Token Op1(Token::Kind kind) {
        auto lex = Token(kind, Loc(), Pos(), 1);
        Advance(1);
        return lex;
    }

    constexpr void SkipBlankspace() {
        while (Match(CharKind::Space, CharKind::LineBreak)) {
            Advance();
        }
    }

    constexpr void SkipComment() {
        // TODO: Handle nested comments /* /* ... */ */
        if (Match("//", "/*")) {
            const std::string_view end = Match("//") ? "\n" : "*/";
            Advance(2);
            while (!IsEof()) {
                if (Match(end)) {
                    Advance(end.size());
                    return;
                }
                Advance();
            }
        }
    }

    constexpr void Advance(uint32_t offset = 1) {
        for (uint32_t i = 0; i < offset && !IsEof(); ++i, ++next_) {
            ++loc_.col;
            if (Match('\r')) {
                loc_.col = 1;
                lineStart_ = next_ + 1;
                ++loc_.line;
            } else if (Match('\n')) {
                lineStart_ = next_ + 1;
                const char* prev = next_ - 1;
                if (prev == text_.data() || *prev != '\r') {
                    loc_.col = 1;
                    ++loc_.line;
                }
            }
        }
    }

    // std::string_view, char, CharKind
    template <class... T>
    constexpr bool Match(T... p) {
        if (IsEof()) {
            return false;
        }
        if ((MatchImpl(p) || ...)) {
            return true;
        }
        return false;
    }

    constexpr bool MatchImpl(std::string_view str) {
        if (str.size() > (uintptr_t)(end_ - Pos())) {
            return false;
        }
        return str == std::string_view(Pos(), Pos() + str.size());
    }

    constexpr bool MatchImpl(char ch) { return *Pos() == ch; }

    constexpr bool MatchImpl(CharKind kind) { return GetKind() == kind; }

    bool TryParseAsFloat(const char* start, Token& token) {
        double val{};
        const std::from_chars_result result =
            std::from_chars(start, Pos(), val);
        if (result.ec != std::error_code()) {
            return false;
        }
        token.value = val;
        return true;
    }

    bool TryParseAsInt(const char* start, Token& token, bool asHex = false) {
        int64_t val{};
        const std::from_chars_result result =
            std::from_chars(start, Pos(), val, asHex ? 16 : 10);
        if (result.ec != std::error_code()) {
            return false;
        }
        token.value = val;
        return true;
    }

    const char* GetLineEnd() {
        const char* ptr = next_;
        while(ptr < end_ && !ASCII::IsLineBreak(*ptr)) {
            ++ptr;
        }
        return ptr;
    }

    constexpr bool IsEof() const { return next_ == end_; }
    constexpr const char* Pos() { return next_; }
    constexpr Location Loc() const { return loc_; }

private:
    constexpr CharKind GetKind() const {
        return (CharKind)ASCII::table[*next_];
    }

private:
    std::string_view text_;
    const char* next_;
    const char* end_;
    const char* lineStart_;
    Location loc_;
    Location lastTokenLoc_;
};

}  // namespace wgsl