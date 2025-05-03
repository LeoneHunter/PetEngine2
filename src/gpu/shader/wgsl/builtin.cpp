#include "builtin.h"
#include "ast_scope.h"

namespace wgsl::ast::internal {

//=============================================================================
// Signature
//=============================================================================

const ast::Type* Signature::Match(std::span<const ast::Type*> args) const {
    // TODO: Handle variadics
    if (ParamsSize() != args.size()) {
        return nullptr;
    }
    auto map = TemplateMap{};
    auto expected = (TypeExpression*)paramHead_;
    auto actual = args.begin();

    for (;;) {
        if (!expected) {
            break;
        }
        if (!expected->Match(*actual, map)) {
            return nullptr;
        }
        expected = expected->next;
        ++actual;
    }
    // Evaluate the return type of this signature specialization
    return returnType_->Evaluate(map);
}

TypeDecl* Signature::FindType(std::string_view name) {
    for (TypeDecl* t = typeHead_; t; t = static_cast<TypeDecl*>(t->next)) {
        if (t->name == name) {
            return t;
        }
    }
    return nullptr;
}



//=============================================================================
// Signature Parser
//=============================================================================

#define TRY_UNWRAP(result, expr)  \
    do {                          \
        auto opt = expr;          \
        if (!opt.has_value()) {   \
            return std::nullopt;  \
        } else {                  \
            result = opt.value(); \
        }                         \
    } while (0)

#define TRY(expr)                \
    do {                         \
        auto opt = expr;         \
        if (!opt) {              \
            return std::nullopt; \
        }                        \
    } while (0)

SignatureParser::SignatureParser(Allocator* al, SymbolTable* symbols)
    : al_(al), symbols_(symbols) {
    DASSERT(al && symbols);
}

std::expected<Signature, Error> SignatureParser::Parse(std::string_view str) {
    auto sig = Signature{};
    CopyInput(str);
    pos_ = 0;
    sig_ = &sig;

    // [T: c | c]
    auto res = ParseTemplateParams();
    if (!res) {
        return std::unexpected(*error_);
    }
    SkipWhitespace();

    // @attr @attr
    res = ParseAttributes();
    if (!res) {
        return std::unexpected(*error_);
    }
    SkipWhitespace();

    // fn myFunc
    auto fnName = ParseFnName();
    if (!fnName) {
        return std::unexpected(*error_);
    }
    sig.name_ = *fnName;
    SkipWhitespace();

    // (e: T, a: f32)
    res = ParseParameters();
    if (!res) {
        return std::unexpected(*error_);
    }
    SkipWhitespace();

    // -> T
    res = ParseReturn();
    if (!res) {
        return std::unexpected(*error_);
    }

    sig_ = nullptr;
    pos_ = 0;
    return std::move(sig);
}

Void SignatureParser::ParseReturn() {
    if (Peek() != '-') {
        return SetError("Expected a '->'");
    }
    Advance();
    if (Peek() != '>') {
        return SetError("Expected a '->'");
    }
    Advance();
    SkipWhitespace();
    auto ident = ParseIdentifier();
    // Parse templated type
    if (Peek() == '<') {
        // TODO: Implement
    }
    // Check if constraint is a reference
    if (TypeDecl* alias = sig_->FindType(ident)) {
        auto expr = al_->Allocate<TemplateReturn>(alias->name);
        sig_->SetReturn(expr);
        return Ok();
    }
    // Check if constraint is a concrete type
    if (ast::Symbol* symbol = symbols_->FindSymbol(ident)) {
        if (ast::Type* concrete = symbol->As<ast::Type>()) {
            auto expr = al_->Allocate<ConcreteReturn>(concrete);
            sig_->SetReturn(expr);
            return Ok();
        }
    }
    return SetError("Unknown return type '{}'", ident);
}

Result<std::string_view> SignatureParser::ParseFnName() {
    if (Peek() != 'f') {
        return SetError("Expected a 'fn' keyword");
    }
    Advance();
    if (Peek() != 'n') {
        return SetError("Expected a 'fn' keyword");
    }
    Advance();
    SkipWhitespace();
    // TODO: Add a templated functions names
    return ParseIdentifier();
}

Void SignatureParser::ParseParameters() {
    if (Peek() != '(') {
        return SetError("Expected '('");
    }
    Advance();

    for (;;) {
        SkipWhitespace();
        if (Peek() == ')') {
            break;
        }
        TRY(ParseParameter());

        SkipWhitespace();
        if (Peek() == ')') {
            Advance();
            break;
        }
        if (Peek() == ',') {
            Advance();
            continue;
        }
        DASSERT_F(Peek() == ',', "Expected ',' or ')'");
    }
    return Ok{};
}

// e: T
Void SignatureParser::ParseParameter() {
    auto name = ParseIdentifier();
    SkipWhitespace();

    if (Peek() != ':') {
        return SetError("Expected ':'");
    }
    Advance();
    SkipWhitespace();

    auto ident = ParseIdentifier();
    if (Peek() == '<') {
        Advance();  // skip '<'
        // Parse template arg list
        // Check if constraint is a templated type
        auto outerName = ident;
        std::vector<TypeExpression*> args;

        for (;;) {
            auto innerIdent = ParseIdentifier();
            TypeExpression* expr;
            TRY_UNWRAP(expr, ResolveSymbol(innerIdent));
            args.push_back(expr);

            if (Peek() == ',') {
                Advance();
                continue;
            } else {
                break;
            }
        }
        // For now only single parameter generics are used
        if (args.size() > 1) {
            return SetError("Expected a generic with one parameter");
        }
        // TODO: Add other generics
        auto expr = al_->Allocate<VectorType>(args.front());
        CreateParam(expr);
    }
    TypeExpression* expr;
    TRY_UNWRAP(expr, ResolveSymbol(ident));
    CreateParam(expr);
    return Ok{};
}

Void SignatureParser::ParseAttributes() {
    while (Peek() == '@') {
        Advance();  // skip @
        auto attr = ParseIdentifier();
        if (attr == "must_use") {
            sig_->isMustUse_ = true;
        } else if (attr == "const") {
            sig_->isConst_ = true;
        } else {
            return SetError("Unknown attribute '{}'", attr);
        }
        SkipWhitespace();
    }
    return Ok{};
}

Void SignatureParser::ParseTemplateParams() {
    if (Peek() != '[') {
        return Ok{};
    }
    Advance();  // skip '['

    for (;;) {
        SkipWhitespace();
        TypeDecl* decl;
        TRY_UNWRAP(decl, ParseTypeDecl());

        sig_->AddTypeDecl(decl);

        SkipWhitespace();
        if (Peek() == ']') {
            Advance();  // skip ']'
            break;
        }
        if (Peek() == ',') {
            Advance();  // skip ','
            continue;
        }
        return SetError("Expected ',' or ']'");
    }
    return Ok{};
}

// T: constraint | constraint ...
Result<TypeDecl*> SignatureParser::ParseTypeDecl() {
    auto name = ParseIdentifier();
    auto* typeDecl = al_->Allocate<TypeDecl>(name);
    if (Peek() != ':') {
        return SetError("Expected a ':'");
    }
    Advance();  // skip ':'
    TRY(ParseConstraints(typeDecl));
    return typeDecl;
}

// constraint | constraint ...
Void SignatureParser::ParseConstraints(TypeDecl* typeDecl) {
    for (;;) {
        SkipWhitespace();
        auto constraintName = ParseIdentifier();
        TypeExpression* constraint = nullptr;

        if (Peek() == '<') {
            Advance();  // skip '<'
            // Parse template arg list
            // Check if constraint is a templated type
            auto outerName = constraintName;
            std::vector<TypeExpression*> args;

            for (;;) {
                auto ident = ParseIdentifier();
                TypeExpression* expr;
                TRY_UNWRAP(expr, ResolveSymbol(ident));
                args.push_back(expr);

                if (Peek() == ',') {
                    Advance();
                    continue;
                }
                if (Peek() == '>') {
                    Advance();
                    break;
                }
                return SetError("Expected ',' or '>'");
            }
            // TODO: Add other generics, matrices and arrays
            // TODO: Handle multiple args
            constraint = al_->Allocate<VectorType>(args.front());
        } else {
            TRY_UNWRAP(constraint, ResolveSymbol(constraintName));
        }
        typeDecl->AddConstraint(constraint);
        SkipWhitespace();

        if (Peek() == '|') {
            Advance();
            continue;
        } else {
            break;
        }
    }
    return Ok{};
}

static const TypeExpression* FindConcept(std::string_view name) {
    static std::map<std::string_view, TypeExpression*> map = {
        {"numeric", Numeric::Instance()},
        {"float", Float::Instance()},
        {"scalar", Scalar::Instance()},
    };
    const auto it = map.find(name);
    if (it == map.end()) {
        return nullptr;
    }
    return it->second;
}

Result<TypeExpression*> SignatureParser::ResolveSymbol(std::string_view ident) {
    // Check if constraint is a concept
    if (const TypeExpression* cpt = FindConcept(ident)) {
        return cpt->Clone(*al_);
    }
    // Check if constraint is an alias to another type
    if (TypeExpression* alias = sig_->FindType(ident)) {
        return alias;
    }
    // Check if constraint is a concrete type
    if (ast::Symbol* symbol = symbols_->FindSymbol(ident)) {
        if (ast::Type* concrete = symbol->As<ast::Type>()) {
            return al_->Allocate<ConcreteType>(concrete);
        }
    }
    return SetError("Unknown constraint '{}'", ident);
}

std::string_view SignatureParser::ParseIdentifier() {
    auto start = pos_;
    size_t len = 0;
    while (isalnum(Peek()) || Peek() == '_') {
        Advance();
        ++len;
    }
    return input_.substr(start, len);
}

void SignatureParser::CreateParam(TypeExpression* expr) {
    auto p = al_->Allocate<Param>(expr);
    sig_->AddParam(p);
}

void SignatureParser::CopyInput(std::string_view input) {
    if(input.empty()) {
        return;
    }
    static_assert(sizeof(char) == 1);
    const auto size = input.size() + 1;
    auto* mem = static_cast<char*>(al_->Allocate(size));
    memcpy(mem, input.data(), size);
    input_ = std::string_view(mem, input.size());
}

}  // namespace wgsl::ast::internal