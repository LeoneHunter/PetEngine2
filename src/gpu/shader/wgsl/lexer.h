#pragma once
#include <array>
#include "token.h"
#include "util.h"

namespace wgsl {

// Text lexer
class Lexer {
public:
    constexpr static uint32_t kLastLine = std::numeric_limits<uint32_t>::max();

    constexpr Lexer(std::string_view code) {
        text_ = code;
        next_ = 0;
        lineOffsets_.push_back(0);
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

    // Get current line for diags
    std::string_view GetLine() { return GetLine(kLastLine); }

    // Get line with index for diags
    std::string_view GetLine(uint32_t lineIdx) {
        // Lines are 1 based
        lineIdx = std::min((uint32_t)lineOffsets_.size() - 1, lineIdx - 1);
        const uint32_t start = lineOffsets_[lineIdx];
        // Find end
        uint32_t end = start + 1;
        while (end < End() && !ASCII::IsLineBreak(At(end))) {
            ++end;
        }
        return MakeStringView(start, end);
    }

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
        uint32_t start = Pos();
        SourceLoc loc = Loc();
        // Scan for type identifying symbols: 0[xX], [.e+-]
        if (Match("0x", "0X")) {
            flags.prefixHex = true;
            flags.intType = true;
            Advance(2);
            start += 2;
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
            Token token;
            TryParseAsFloat(&At(start), token);
            if (flags.suffixF) {
                token.kind = Token::Kind::LitFloat;
            } else if (flags.suffixH) {
                token.kind = Token::Kind::LitHalf;
            } else {
                token.kind = Token::Kind::LitAbstrFloat;
            }
            token.loc = SourceLoc(loc, Pos() - start);
            return token;
        }
        if (flags.intType) {
            Token token;
            TryParseAsInt(&At(start), token, flags.prefixHex);
            if (flags.suffixU) {
                token.kind = Token::Kind::LitUint;
            } else if (flags.suffixI) {
                token.kind = Token::Kind::LitInt;
            } else {
                token.kind = Token::Kind::LitAbstrInt;
            }
            token.loc = SourceLoc(loc, Pos() - start);
            return token;
        }
        return Token::Invalid(loc);
    }

    constexpr Token ParseLetter() {
        for (const std::string_view keyword : kKeywords) {
            if (Match(keyword)) {
                const auto out = Token(Token::Kind::Keyword,
                                       SourceLoc(Loc(), keyword.size()),
                                       &Peek(), keyword.size());
                Advance(keyword.size());
                return out;
            }
        }
        for (const std::string_view reserved : kReserved) {
            if (Match(reserved)) {
                const auto out = Token(Token::Kind::Reserved,
                                       SourceLoc(Loc(), reserved.size()),
                                       &Peek(), reserved.size());
                Advance(reserved.size());
                return out;
            }
        }
        if (Match("true")) {
            const auto tok =
                Token(Token::Kind::LitBool, SourceLoc(Loc(), 4), 1LL);
            Advance(4);
            return tok;
        }
        if (Match("false")) {
            const auto tok =
                Token(Token::Kind::LitBool, SourceLoc(Loc(), 5), 0LL);
            Advance(5);
            return tok;
        }
        const uint32_t start = Pos();
        SourceLoc loc = Loc();
        while (Match(CharKind::Digit, CharKind::Letter, '_')) {
            Advance();
        }
        const uint32_t len = Pos() - start;
        return Token(Token::Kind::Ident, SourceLoc(loc, len),
                     MakeStringView(start, start + len));
    }

    constexpr Token ParsePunctuation() {
        switch (Peek()) {
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
        auto lex = Token(kind, SourceLoc(Loc(), 3), &Peek(), 3);
        Advance(3);
        return lex;
    }

    constexpr Token Op2(Token::Kind kind) {
        auto lex = Token(kind, SourceLoc(Loc(), 2), &Peek(), 2);
        Advance(2);
        return lex;
    }

    constexpr Token Op1(Token::Kind kind) {
        auto lex = Token(kind, Loc(), &Peek(), 1);
        Advance(1);
        return lex;
    }

    constexpr void SkipBlankspace() {
        while (Match(CharKind::Space, CharKind::LineBreak)) {
            Advance();
        }
    }

    constexpr void SkipComment() {
        if (Match("/*")) {
            Advance(2);
            while (!IsEof() && !Match("*/")) {
                // Nested comments /* /* ... */ */
                if (Match("/*")) {
                    SkipComment();
                }
                Advance();
            }
            Advance(2);
        } else if (Match("//")) {
            Advance(2);
            while (!IsEof() && !Match(CharKind::LineBreak)) {
                Advance();
            }
            Advance();
        }
    }

    constexpr void Advance(uint32_t offset = 1) {
        for (uint32_t i = 0; i < offset && !IsEof(); ++i, ++next_) {
            if (Match(CharKind::LineBreak)) {
                auto nextLineStart = next_ + 1;
                if (Match("\r\n")) {
                    ++next_;
                    ++nextLineStart;
                }
                if (nextLineStart < End()) {
                    lineOffsets_.push_back(nextLineStart);
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
        if (str.size() > (End() - Pos())) {
            return false;
        }
        return str == std::string_view(&Peek(), str.size());
    }

    constexpr bool MatchImpl(char ch) { return Peek() == ch; }

    constexpr bool MatchImpl(CharKind kind) { return GetKind() == kind; }

    bool TryParseAsFloat(const char* start, Token& token) {
        double val{};
        const std::from_chars_result result =
            std::from_chars(start, &Peek(), val);
        if (result.ec != std::error_code()) {
            return false;
        }
        token.value = val;
        return true;
    }

    bool TryParseAsInt(const char* start, Token& token, bool asHex = false) {
        int64_t val{};
        const std::from_chars_result result =
            std::from_chars(start, &Peek(), val, asHex ? 16 : 10);
        if (result.ec != std::error_code()) {
            return false;
        }
        token.value = val;
        return true;
    }

    constexpr std::string_view MakeStringView(uint32_t offset,
                                              uint32_t end) const {
        return {&text_[offset], end - offset};
    }

    constexpr SourceLoc Loc() const {
        return {(uint32_t)lineOffsets_.size(), Pos() - GetLineStart() + 1, 1};
    }

    constexpr bool IsEof() const { return Pos() == End(); }

    constexpr uint32_t GetLineStart() const { return lineOffsets_.back(); }
    constexpr uint32_t Pos() const { return next_; }
    constexpr uint32_t End() const { return text_.size(); }

    constexpr const char& At(size_t offset) const { return text_[offset]; }
    constexpr const char& Peek() const { return text_[next_]; }

private:
    constexpr CharKind GetKind() const {
        return (CharKind)ASCII::table[Peek()];
    }

private:
    std::string_view text_;
    std::vector<uint32_t> lineOffsets_;
    uint32_t next_;
};

}  // namespace wgsl