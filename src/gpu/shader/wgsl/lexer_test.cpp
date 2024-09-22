#include "lexer.h"
#include <doctest.h>

using namespace wgsl;
using Kind = Token::Kind;

namespace {
void CheckTokenKind(const std::vector<Token>& input,
                    const std::vector<Token::Kind>& kinds) {
    CHECK_EQ(input.size(), kinds.size());
    for (size_t i = 0; i < input.size(); ++i) {
        CHECK_MESSAGE(
            input[i].GetKind() == kinds[i],
            std::format("\n  Expected: Kind::{}\n  Got:      Kind::{}",
                        to_string(kinds[i]), to_string(input[i].GetKind())));
    }
}

template<class... K>
void Test(std::string_view text, K... kind) {
    std::vector<Kind> expected;
    (expected.push_back(kind), ...);
    auto lexer = Lexer(text);
    std::vector<Token> tokens = lexer.ParseAll();
    CheckTokenKind(tokens, expected);
}

}  // namespace

TEST_CASE("[wgsl::Lexer] Numbers") {
    // clang-format off
    Test(R"(
            123   12.32 0.43f 0x32
            0X43  0.e+4 1e-3  43f
            12u
        )",
        Kind::LitAbstrInt, Kind::LitAbstrFloat, Kind::LitFloat,      Kind::LitAbstrInt,   
        Kind::LitAbstrInt, Kind::LitAbstrFloat, Kind::LitAbstrFloat, Kind::LitFloat, 
        Kind::LitUint,     Kind::EOF
    );
    // clang-format on
}

TEST_CASE("[wgsl::Lexer] Punctuation") {
    // clang-format off
    Test(R"(
            !@%^&* 
            *=()--- 
            -=+++ += 
            ;:{}[]
            / /=>><< 
        )",
        Kind::Negation, Kind::Attr, Kind::Mod, Kind::Xor, Kind::And, Kind::Mul,
        Kind::MulEqual, Kind::OpenParen, Kind::CloseParen, Kind::MinusMinus, Kind::Minus,
        Kind::MinusEqual, Kind::PlusPlus, Kind::Plus, Kind::PlusEqual,
        Kind::Semicolon, Kind::Colon, Kind::OpenBrace, Kind::CloseBrace, Kind::OpenBracket, Kind::CloseBracket,
        Kind::Div, Kind::DivEqual, Kind::RightShift, Kind::LeftShift, Kind::EOF
    );
    // clang-format on
}

TEST_CASE("[wgsl::Lexer] Comments") {
    // clang-format off
    Test(R"(
            ident1
            // comment
            ident2 /* comment */
            ident3
            /* commment */ident4
        )",
        Kind::Ident, Kind::Ident, Kind::Ident, Kind::Ident, Kind::EOF
    );
    // clang-format on
}

TEST_CASE("[wgsl::Lexer] OnlyComments") {
    // clang-format off
    Test(R"(
            // comment/* comment */

            /* commment */     // comment

            // comment
        )",
        Kind::EOF
    );
    // clang-format on
}

TEST_CASE("[wgsl::Lexer] Struct") {
    // clang-format off
    Test(R"(
            struct Particle {
                pos : vec2f,
                vel : vec2f,
            };
        )",
        Kind::Keyword,    Kind::Ident,      Kind::OpenBrace,      
        Kind::Ident,      Kind::Colon,      Kind::Ident,     Kind::Comma,
        Kind::Ident,      Kind::Colon,      Kind::Ident,     Kind::Comma,
        Kind::CloseBrace, Kind::Semicolon,  Kind::EOF
    );
    // clang-format on
}

TEST_CASE("[wgsl::Lexer] Declaration") {
    // clang-format off
    Test(R"(
            @binding(1) 
            var<storage, read_write> 
            particlesA : Particles;
        )",
        Kind::Attr,    Kind::Ident,    Kind::OpenParen, Kind::LitAbstrInt, Kind::CloseParen, 
        Kind::Keyword, Kind::LessThan, Kind::Ident,     Kind::Comma,       Kind::Ident,      Kind::GreaterThan,
        Kind::Ident,   Kind::Colon,    Kind::Ident,     Kind::Semicolon,   Kind::EOF
    );
    // clang-format on
}