#pragma once
#include "base/bump_alloc.h"
#include "base/common.h"

#include "ast_node.h"
#include "ast_type.h"

#include <unordered_map>

namespace wgsl::ast {

class SymbolTable {
public:
    ast::Symbol* FindSymbol(std::string_view name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            return it->second;
        }
        for (auto* ancestor = parent_; ancestor; ancestor = ancestor->parent_) {
            if (auto result = ancestor->FindSymbol(name)) {
                return result;
            }
        }
        return nullptr;
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

    SymbolTable(SymbolTable* parent) : keyAlloc_(512) {
        parent_ = parent;
        if (parent) {
            prevSibling_ = parent->lastChild_;
            parent->lastChild_ = this;
        }
    }

private:
    SymbolTable* parent_ = nullptr;
    // forward lists, reversed to declaration order
    SymbolTable* lastChild_ = nullptr;
    SymbolTable* prevSibling_ = nullptr;
    // identifier to declaration
    std::unordered_map<std::string_view, ast::Symbol*> symbols_;
    // Owns key strings
    BumpAllocator keyAlloc_;
};

// Global scope with top level declarations
// Root node of ast tree
class GlobalScope final : public Node {
public:
    SymbolTable* symbolTable;
    ProgramList<Symbol*> decls;

    static constexpr auto kStaticType = NodeType::GlobalScope;

    GlobalScope(SymbolTable* symbolTable)
        : Node(SourceLoc(), kStaticType), symbolTable(symbolTable) {}

    Symbol* FindSymbol(std::string_view name) {
        return symbolTable->FindSymbol(name);
    }

    // Declares a source located symbol
    void Declare(std::string_view name, Symbol* symbol) {
        DASSERT(!name.empty() && symbol);
        symbolTable->InsertSymbol(name, symbol);
        decls.push_back(symbol);
    }

    // Declares a builtin symbol not found in source
    void PreDeclare(std::string_view name, Symbol* symbol) {
        DASSERT(!name.empty() && symbol);
        symbolTable->InsertSymbol(name, symbol);
    }
};

}  // namespace wgsl::ast
