#include "ast_type.h"
#include "base/bump_alloc.h"

#include <map>

namespace wgsl::ast {
class SymbolTable;
class Type;
}  // namespace wgsl::ast

namespace wgsl::ast::internal {

// Allocator for expression nodes
// Nodes are immutable so we dont need to delete them
using Allocator = BumpAllocator;

// Maps a template param to a concrete type
// T -> ast::Scalar ("f32")
using TemplateMap = std::map<std::string_view, const ast::Type*>;

// Base class for type expressions
class TypeExpression {
public:
    virtual ~TypeExpression() = default;
    virtual bool Match(const ast::Type* type, TemplateMap& map) const = 0;
    virtual TypeExpression* Clone(Allocator& al) const { return nullptr; }

    // Linked list
    TypeExpression* next = nullptr;
};

// Checks whether a type is a numeric type
class Numeric : public TypeExpression {
public:
    bool Match(const ast::Type* type, TemplateMap& map) const final {
        const auto scalar = type->As<ast::Scalar>();
        return scalar && scalar->IsArithmetic();
    }

    TypeExpression* Clone(Allocator& al) const final {
        return al.Allocate<Numeric>();
    }

    static Numeric* Instance() {
        static Numeric i;
        return &i;
    }
};

// Checks whether a type is a float type
class Float : public TypeExpression {
public:
    bool Match(const ast::Type* type, TemplateMap& map) const final {
        const auto scalar = type->As<ast::Scalar>();
        return scalar && scalar->IsFloat();
    }

    TypeExpression* Clone(Allocator& al) const final {
        return al.Allocate<Float>();
    }

    static Float* Instance() {
        static Float i;
        return &i;
    }
};

// Checks whether a type is a scalar type
class Scalar : public TypeExpression {
public:
    bool Match(const ast::Type* type, TemplateMap& map) const final {
        return type->Is<ast::Scalar>();
    }

    TypeExpression* Clone(Allocator& al) const final {
        return al.Allocate<Scalar>();
    }

    static Scalar* Instance() {
        static Scalar i;
        return &i;
    }
};

// Concrete predefined type
// E.g. f32, i32, bool
class ConcreteType : public TypeExpression {
public:
    ConcreteType(const ast::Type* t) : type(t) {}

    bool Match(const ast::Type* t, TemplateMap& map) const final {
        return type == t;
    }

    const ast::Type* type;
};


// Vector type expression
// Checks whether a type is a vector of a specified value type
class VectorType : public TypeExpression {
public:
    VectorType(TypeExpression* et = nullptr) : elementType(et) {}

    bool Match(const ast::Type* type, TemplateMap& map) const final {
        const auto vec = type->As<ast::Vec>();
        if (vec) {
            return elementType->Match(vec->valueType, map);
        }
        return false;
    }

    TypeExpression* elementType;
};

// A declaration of a type
// Defines a list of rules that evaluate this type
// For example the declaration "T : numeric | vec<numeric>"
//   defines a type which is constrained by two rules
class TypeDecl : public TypeExpression {
public:
    TypeDecl(std::string_view name) : name(name) {}

    bool Match(const ast::Type* type, TemplateMap& map) const final {
        // Check if match to the already substituted type
        if (auto it = map.find(name); it != map.end()) {
            if (it->second != type) {
                return false;
            }
            return true;
        }
        // Try to substitute
        for (TypeExpression* rule = constraints; rule; rule = rule->next) {
            if (rule->Match(type, map)) {
                map[name] = type;
                return true;
            }
        }
        return false;
    }

    void AddConstraint(TypeExpression* expr) {
        expr->next = constraints;
        constraints = expr;
    }

    std::string_view name;
    TypeExpression* constraints = nullptr;
};

// Signature parameter
class Param : public TypeExpression {
public:
    Param(TypeExpression* expr) : expr(expr) {}

    bool Match(const ast::Type* type, TemplateMap& map) const final {
        return expr->Match(type, map);
    }

    TypeExpression* expr = nullptr;
};

// Base class for return type evaluators
class ReturnExpression {
public:
    virtual ~ReturnExpression() = default;
    virtual const ast::Type* Evaluate(TemplateMap& map) const = 0;
};

class ConcreteReturn : public ReturnExpression {
public:
    ConcreteReturn(const ast::Type* type) : type(type) {}
    const ast::Type* Evaluate(TemplateMap& map) const override { return type; }
    const ast::Type* type;
};

class TemplateReturn : public ReturnExpression {
public:
    TemplateReturn(std::string_view name) : name(name) {}

    const ast::Type* Evaluate(TemplateMap& map) const override {
        if (auto it = map.find(name); it != map.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::string_view name;
};

// Function signature representation
// Containts a tree of expressions that can be evaluated at runtime
//   to check if some type list matches the signature
// Template parameters in the signature expanded as well
// Example signature: [S: float] length (T: S | vec<S>) S
// NOTE: Signature uses external allocator and references symbols from
//   program symbol table, hence it's owned by the parent program
class Signature {
public:
    friend class SignatureParser;

    Signature() = default;

    // No copy because cumbersome to copy a tree of nodes
    Signature(Signature&&) = default;
    Signature& operator=(Signature&&) = default;

    // Try to match the parameters to the signature
    // Returns a resulted return type of the function if successful
    const ast::Type* Match(std::span<const ast::Type*> args) const;

private:
    TypeDecl* FindType(std::string_view name);

    void AddParam(Param* param) {
        param->next = paramHead_;
        paramHead_ = param;
    }

    void SetReturn(ReturnExpression* ret) { returnType_ = ret; }

    void AddTypeDecl(TypeDecl* type) {
        type->next = typeHead_;
        typeHead_ = type;
    }

    size_t ParamsSize() const {
        size_t size = 0;
        for (TypeExpression* p = paramHead_; p; p = p->next) {
            ++size;
        }
        return size;
    }

private:
    std::string_view name_;
    // Type declarations which contain rules for a type evaluation
    // For example T : number | vec<number>
    TypeDecl* typeHead_ = nullptr;
    // List of the function parameters
    // Each parameter could be a reference to a type declaraion
    Param* paramHead_ = nullptr;
    ReturnExpression* returnType_ = nullptr;
    bool isConst_ = false;
    bool isMustUse_ = false;
};


template <class T>
using Result = std::optional<T>;
using Void = std::optional<bool>;
using Ok = bool;

// Contains error info from the parser
struct Error {
    size_t pos;
    std::string msg;
};

// Parses and creates signatures from strings
class SignatureParser {
public:
    SignatureParser(Allocator* al, SymbolTable* symbols);

    std::expected<Signature, Error> Parse(std::string_view str);

private:
    Void ParseAttributes();

    Void ParseReturn();

    Void ParseTemplateParams();

    Result<TypeDecl*> ParseTypeDecl();

    Result<std::string_view> ParseFnName();

    Void ParseParameters();

    Void ParseParameter();

    Void ParseConstraints(TypeDecl* typeDecl);

    // Resolve the identifier: T, f32, numeric
    Result<TypeExpression*> ResolveSymbol(std::string_view ident);

    std::string_view ParseIdentifier();

    void CreateParam(TypeExpression* expr);

    // Copies the inputs string into the 'al_'
    void CopyInput(std::string_view input);

    void SkipWhitespace() {
        while (Peek() == ' ' || Peek() == '\t') {
            Advance();
        }
    }

    char Peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }

    char Advance() { return pos_ < input_.size() ? input_[pos_++] : '\0'; }

    template <class... Args>
    std::nullopt_t SetError(std::format_string<Args...> fmt, Args&&... args) {
        const auto msg = std::format(fmt, std::forward<Args>(args)...);
        error_ = Error{pos_, msg};
        return std::nullopt;
    }

private:
    std::string_view input_;
    size_t pos_ = 0;
    std::optional<Error> error_;

    Allocator* al_ = nullptr;
    SymbolTable* symbols_ = nullptr;
    Signature* sig_ = nullptr;
};

}  // namespace wgsl::ast::internal