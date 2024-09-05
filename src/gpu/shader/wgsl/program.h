#pragma once
#include "base/common.h"
#include "base/bump_alloc.h"

#include "ast_expression.h"
#include "ast_variable.h"
#include "ast_attribute.h"

class TreePrinter;

namespace wgsl {

// Parsed wgsl code into a ast tree with minor transforms
class Program {
public:
    static std::unique_ptr<Program> Create(std::string_view code);

    std::string GetDiagsAsString();
    
    void PrintAst(TreePrinter* printer) const;

public:
    friend class ProgramBuilder;

    struct DiagMsg {
        enum class Type {
            Error,
            Warning,
        };

        LocationRange loc;
        std::string msg;
        Type type;
    };
    // Copied source code
    std::string sourceCode_;
    BumpAllocator alloc_;
    std::vector<ast::Variable*> globalDecls_;
    // Diagnostic messages: errors and warnings
    std::vector<DiagMsg> diags_;
};

} // namespace wgsl