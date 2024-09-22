#pragma once
#include "ast_function.h"
#include "ast_type.h"
#include "common.h"
#include "lexer.h"

#include <functional>
#include "base/bump_alloc.h"

namespace wgsl {

struct Writer {
    std::string* str;
    Writer(std::string* str) : str(str) {}
    Writer& operator<<(std::string_view s) {
        str->append(s);
        return *this;
    }
};


// A type name
struct Type {
    std::string_view name;
    std::vector<Type> templateParams;
};

// Template paramete declaration
struct TemplateParam {
    std::string_view name;
    // Allowed types
    std::vector<Type> constraints;
};

// Function parameter
struct Param {
    std::string_view name;
    Type type;
};

// A parsed signature of a builtin function
// Used to validate function parameters during wgsl program parsing
// T = float, int, i32, u32, f32, bool
// @const @must_use fn i32(e: T)-> i32
struct BuiltinFuncSignature {
    bool mustUse;
    bool isConst;
    std::string name;
    std::vector<TemplateParam> templateParams;
    std::vector<Param> params;
    Type ret;
};

// Writes a static declaraion in the format:
//
//   static auto f = BuiltinFuncSignature{
//       .mustUse = false,
//       .isConst = false,
//       .name = "abs",
//       .templateParams = {{"T", {{"vecN", "i32"}, {"i32", ""}}}},
//       .params = {{"e", {"i32", ""}}, {"r", "T"}},
//       .ret = {"vecN", "T"},
//   };
//
// struct SignatureWriter {
//     static void Write(const BuiltinFuncSignature& sig, Writer& s) {
//         s << "static auto k" << sig.name << " = BuiltinFuncSignature {\n";
//         s << "  .mustUse = " << (sig.mustUse ? "true" : "false") << ",\n";
//         s << "  .isConst = " << (sig.isConst ? "true" : "false") << ",\n";
//         s << "  .name = \"" << sig.name << "\",\n";
//         WriteTemplate(sig, s);
//         WriteParams(sig, s);
//         s << "  .ret = ";
//         WriteType(sig.ret, s);
//         s << ",\n";
//         s << "};\n";
//     }

// private:
//     static void WriteTemplate(const BuiltinFuncSignature& sig, Writer& s) {
//         s << "  .templateParams = {";
//         for (const auto [param, types] : sig.templateParams) {
//             s << "{\"" << param << "\", {";
//             for (const auto type : types) {
//                 WriteType(type, s);
//                 s << ", ";
//             }
//             s << "}},";
//         }
//         s << "},\n";
//     }

//     static void WriteParams(const BuiltinFuncSignature& sig, Writer& s) {
//         s << "  .params = {";
//         for (const auto [param, type] : sig.params) {
//             s << "{\"" << param << "\", ";
//             WriteType(type, s);
//             s << "}, ";
//         }
//         s << "},\n";
//     }

//     static void WriteType(const BuiltinFuncSignature::Type& type, Writer& s)
//     {
//         s << "{\"" << type.name << "\", \"" << type.nestedName << "\"}";
//     }
// };

class SignatureBuilder {
public:
    void SetMustUse() { sig.mustUse = true; }
    void SetConst() { sig.isConst = true; }
    void SetName(std::string_view name) { sig.name = name; }

    void OpenTemplate(std::string_view name) {
        auto& p = sig.templateParams.emplace_back();
        p.name = name;
    }

    void AddTemplateParamType(const Type& type) {
        DASSERT(!sig.templateParams.empty());
        sig.templateParams.back().constraints.push_back(type);
    }

    void AddParameter(std::string_view name, const Type& type) {
        auto& p = sig.params.emplace_back();
        p.name = name;
        p.type = type;
    }

    void SetRetType(const Type& type) { sig.ret = type; }

    BuiltinFuncSignature Build() { return std::move(sig); }

private:
    BuiltinFuncSignature sig;
};

// Parses a templated signature declaration of a builtin function
// Format matches the signature declaration at
// https://www.w3.org/TR/WGSL/#builtin-functions
//
//   template_type '=' | 'is' type_specifier (',' | 'or' | ', or'
//     type_specifier)*
//   ( '@' attr )? 'fn' '(' ident ':' type_specifier (',' type_specifier)* ')'
//      '->' type_specifier ';'?
// E.g.
//      S = AbstractFloat, f32, f16
//      T = S, vecN<S>
//      @const @must_use fn asin(e: T) -> T;
//
class SignatureParser {
public:
    struct Error {
        std::string msg;
        SourceLoc loc;
    };
    using ParseResult = std::expected<BuiltinFuncSignature, Error>;

    ParseResult Parse(std::string_view s) {
        auto lexer = Lexer(s);
        lexer_ = &lexer;
        Advance();

        while (!ShouldStop()) {
            if (Expect(Token::Kind::Attr)) {
                ParseAttribute();
            } else if (Expect("fn")) {
                ParseFunction();
            } else if (Match(Token::Kind::Ident)) {
                ParseTemplate();
            } else {
                SetError("unexpected token");
            }
        }
        if (errored_) {
            return std::unexpected(err_);
        }
        return ParseResult(builder_.Build());
    }

private:
#define TRY(EXPR, ERROR) \
    if (!EXPR)           \
        return ERROR;

    bool ParseTemplate() {
        builder_.OpenTemplate(GetSourceView());
        Advance();
        if (!Expect("is") && !Expect("=")) {
            return SetError("expected a '=' or 'is'");
        }
        for (;;) {
            Type type;
            TRY(ParseTypeName(type), false);
            builder_.AddTemplateParamType(type);

            // Next param: , ||
            if (Expect(Token::Kind::Comma) || Expect("or") ||
                Expect(Token::Kind::OrOr)) {
                // Optional 'or'. E.g. i32, u32, or f32
                Expect("or");
                continue;
            } else if (Match(Token::Kind::Ident) || Match(Token::Kind::Attr) ||
                       Expect(Token::Kind::Semicolon)) {
                // Start of next line: ident @ fn ;
                break;
            } else {
                return SetError("expected a ',', '||', '@' or ident");
            }
        }
        return true;
    }

    bool ParseFunction() {
        TRY(Match(Token::Kind::Ident), SetError("expected a func name"));
        builder_.SetName(GetSourceView());
        Advance();
        TRY(Expect(Token::Kind::OpenParen), SetError("expected a ("));
        TRY(ParseParameters(), false);
        TRY(Expect(Token::Kind::Arrow), SetError("expected a ->"));
        TRY(ParseRetTypeName(), false);
        // Optional ';'
        Expect(Token::Kind::Semicolon);
        return true;
    }

    bool ParseAttribute() {
        if (Expect("must_use")) {
            builder_.SetMustUse();
            return true;
        } else if (Expect("const")) {
            builder_.SetConst();
            return true;
        }
        return SetError("expected an attribute name");
    }

    bool ParseParameters() {
        for (;;) {
            TRY(Match(Token::Kind::Ident), SetError("expected an ident"));
            const std::string_view name = GetSourceView();
            Advance();
            TRY(Expect(Token::Kind::Colon), SetError("expected a :"));
            Type type;
            TRY(ParseTypeName(type), false);
            builder_.AddParameter(name, type);

            if (Expect(Token::Kind::Comma)) {
                continue;
            } else if (Expect(Token::Kind::CloseParen)) {
                break;
            } else {
                return SetError("expected a ')' or ','");
            }
        }
        return true;
    }

    bool ParseRetTypeName() {
        Type type;
        TRY(ParseTypeName(type), false);
        builder_.SetRetType(type);
        return true;
    }

    bool ParseTypeName(Type& outType) {
        TRY(Match(Token::Kind::Ident), SetError("expected a ident"));
        outType.name = GetSourceView();
        Advance();
        if (Expect(Token::Kind::LessThan)) {
            for (;;) {
                Type param;
                TRY(ParseTypeName(param), false);
                outType.templateParams.push_back(param);

                if (Expect(Token::Kind::GreaterThan)) {
                    break;
                } else if (Expect(Token::Kind::Comma)) {
                    continue;
                } else {
                    return SetError("expected a > or ,");
                }
            }
        }
        return true;
    }

#undef TRY

    bool Advance() { return lexer_->ParseNext(token_) && !errored_; }

    bool Match(std::string_view str) { return token_.Source() == str; }

    bool Match(Token::Kind kind) { return token_.kind == kind; }

    bool Expect(std::string_view str) {
        if (token_.Source() == str) {
            Advance();
            return true;
        }
        return false;
    }

    bool Expect(Token::Kind kind) {
        if (token_.kind == kind) {
            Advance();
            return true;
        }
        return false;
    }

    std::string_view GetSourceView() { return token_.Source(); }

    bool SetError(std::string_view msg) {
        err_.msg = msg;
        err_.loc = token_.loc;
        errored_ = true;
        return false;
    }

    bool ShouldStop() { return errored_ || token_.kind == Token::Kind::EOF; }

private:
    Lexer* lexer_ = nullptr;
    Token token_;
    SignatureBuilder builder_;
    Error err_;
    bool errored_ = false;
};

}  // namespace wgsl