#pragma once
#include "base/bump_alloc.h"
#include "base/common.h"

#include "ast_attribute.h"
#include "ast_expression.h"
#include "ast_type.h"
#include "ast_variable.h"

#include <unordered_map>

class TreePrinter;

namespace wgsl {

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

    std::string GetDiagsAsString();
    const std::vector<DiagMsg>& GetDiags();
    void PrintAst(TreePrinter* printer) const;

    // Find symbol
    const ast::Node* FindSymbol(std::string_view name,
                                std::string_view scope = "") const;

private:
    Program();

    // Creates symbols for: i32, u32, f32 etc.
    void CreateBuiltinTypes();

private:
    struct Scope {
        Scope* parent = nullptr;
        // forward lists, reversed to declaration order
        Scope* lastChild = nullptr;
        Scope* prevSibling = nullptr;

        std::vector<ast::Variable*> variables;
        // type ident -> ast::Type* decl: (struct, alias, builtin)
        // ident -> ast::Variable* decl: (const, var, override)
        // fn ident -> ast::Function* decl: (fn)
        // identifier to declaration
        std::unordered_map<std::string_view, ast::Node*> symbols;

        Scope(Scope* parent) {
            this->parent = parent;
            if (parent) {
                this->prevSibling = parent->lastChild;
                parent->lastChild = this;
            }
        }

        // returns true if successful
        bool InsertSymbol(std::string_view name, ast::Node* decl) {
            return symbols.emplace(name, decl).second;
        }

        ast::Node* FindSymbol(std::string_view name) {
            auto it = symbols.find(name);
            if (it != symbols.end()) {
                return it->second;
            }
            return nullptr;
        }
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