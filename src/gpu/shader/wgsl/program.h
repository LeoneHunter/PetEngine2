#pragma once
#include "base/bump_alloc.h"
#include "base/common.h"

#include "ast_attribute.h"
#include "ast_expression.h"
#include "ast_type.h"
#include "ast_variable.h"
#include "ast_function.h"

#include <unordered_map>

class TreePrinter;

namespace wgsl {

namespace internal {
struct ProgramAllocBase;
}

// Parsed wgsl code into a ast tree with minor transforms
class Program {
public:
    // Diagnostic messages: errors and warnings
    struct DiagMsg {
        enum class Type {
            Error,
            Warning,
        };

        Type type;
        SourceLoc loc;
        ErrorCode code;
        std::string msg;
    };

public:
    static std::unique_ptr<Program> Create(std::string_view code);
    ~Program();

    std::string GetDiagsAsString();
    const std::vector<DiagMsg>& GetDiags() const { return diags_; }
    const std::string_view GetSource() const { return sourceCode_; }

    void PrintAst(TreePrinter* printer) const;

    // Find symbol
    const ast::Symbol* FindSymbol(std::string_view name,
                                  std::string_view scope = "") const;

private:
    Program();

    friend struct internal::ProgramAllocBase;
    static Program* GetCurrent();

    template <class T, class... Args>
        requires std::constructible_from<T, Args...>
    T* Allocate(Args&&... args) {
        return alloc_.Allocate<T>(std::forward<Args>(args)...);
    }

    // Copy string into program memory
    std::string_view EmbedString(const std::string& src) {
        static_assert(sizeof(char) == 1);
        const auto size = src.size() + 1;
        auto* mem = static_cast<char*>(alloc_.Allocate(size));
        memcpy(mem, src.data(), size);
        return std::string_view(mem, src.size());
    }

private:
    class Scope {
    public:
        Scope(Scope* parent) : keyAlloc_(8 * 1024) {
            parent_ = parent;
            if (parent) {
                prevSibling_ = parent->lastChild_;
                parent->lastChild_ = this;
            }
        }

        void PushDecl(ast::Variable* var) {
            variables_.push_back(var);
        }

        void PushDecl(ast::Struct* decl) {
            structs_.push_back(decl);
        }

        void PushDecl(ast::Function* decl) {
            functions_.push_back(decl);
        }

        // Returns true if successful
        // Copies a string into internal memory so any source is allowed
        bool InsertSymbol(std::string_view name, ast::Symbol* decl) {
            static_assert(sizeof(char) == 1);
            const auto size = name.size() + 1;
            auto* mem = static_cast<char*>(keyAlloc_.Allocate(size));
            memcpy(mem, name.data(), size);
            const auto res =
                symbols_.emplace(std::string_view(mem, name.size()), decl);
            return res.second;
        }

        ast::Symbol* FindSymbol(std::string_view name) {
            auto it = symbols_.find(name);
            if (it != symbols_.end()) {
                return it->second;
            }
            Scope* ancestor = this;
            while (bool(ancestor = ancestor->parent_)) {
                if (auto result = ancestor->FindSymbol(name)) {
                    return result;
                }
            }
            return nullptr;
        }

        ast::Type* FindType(std::string_view name) {
            if (auto* node = FindSymbol(name)) {
                return node->As<ast::Type>();
            }
            return nullptr;
        }

        bool IsGlobal() const { return parent_ == nullptr; }
        bool IsFunction() const { return !IsGlobal(); }

    private:
        Scope* parent_ = nullptr;
        // forward lists, reversed to declaration order
        Scope* lastChild_ = nullptr;
        Scope* prevSibling_ = nullptr;

        std::vector<ast::Variable*> variables_;
        std::vector<ast::Struct*> structs_;
        std::vector<ast::Function*> functions_;
        // type ident -> ast::Type* decl: (struct, alias, builtin)
        // ident -> ast::Variable* decl: (const, var, override)
        // fn ident -> ast::Function* decl: (fn)
        // identifier to declaration
        std::unordered_map<std::string_view, ast::Symbol*> symbols_;
        BumpAllocator keyAlloc_;
    };

public:
    friend class ProgramBuilder;

    // Copied source code
    std::string sourceCode_;
    BumpAllocator alloc_;
    // Tree of scopes
    Scope* globalScope_ = nullptr;
    std::vector<DiagMsg> diags_;
};

}  // namespace wgsl