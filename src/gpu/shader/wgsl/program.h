#pragma once
#include "base/bump_alloc.h"
#include "base/common.h"
#include "common.h"

class TreePrinter;

namespace wgsl {

namespace ast {
class GlobalScope;
class Symbol;
}

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

public:
    friend class ProgramBuilder;

    // Copied source code
    std::string sourceCode_;
    BumpAllocator alloc_;
    // Tree of scopes
    // Owned by alloc_
    ast::GlobalScope* globalScope_ = nullptr;
    std::vector<DiagMsg> diags_;
};

}  // namespace wgsl