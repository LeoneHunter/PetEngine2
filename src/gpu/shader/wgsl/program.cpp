#include "program.h"
#include "lexer.h"
#include "parser.h"
#include "program_builder.h"

#include "ast_printer.h"
#include "ast_scope.h"

#include "base/tree_printer.h"

namespace wgsl {

thread_local Program* currentProgram = nullptr;

std::unique_ptr<Program> Program::Create(std::string_view code) {
    auto builder = ProgramBuilder();
    builder.Build(code);
    auto program = builder.Finalize();
    return program;
}

Program::Program() {
    // Nested program building doesn't make sense
    DASSERT(!currentProgram);
    currentProgram = this;
    auto* symbols = alloc_.Allocate<ast::SymbolTable>(nullptr);
    globalScope_ = alloc_.Allocate<ast::GlobalScope>(symbols);
}

Program::~Program() {
    currentProgram = nullptr;
}

Program* Program::GetCurrent() {
    DASSERT(currentProgram);
    return currentProgram;
}

std::string Program::GetDiagsAsString() {
    std::string out;
    for (uint32_t i = 0; i < diags_.size(); ++i) {
        const auto& msg = diags_[i];
        const auto type =
            msg.type == DiagMsg::Type::Error ? "error" : "warning";
        const auto loc = msg.loc;
        out += std::format("\n{} [Ln {}, Col {}]: {}", type, loc.line, loc.col,
                           msg.msg);
    }
    return out;
}

void Program::PrintAst(TreePrinter* printer) const {
    ast::AstPrinter astPrinter;
    // for (const ast::Variable* var : globalScope_->variables) {
    //     astPrinter.Print(printer, var);
    // }
}

const ast::Symbol* Program::FindSymbol(std::string_view name,
                                       std::string_view scope) const {
    DASSERT(scope.empty() && "Unimplemented");
    return globalScope_->FindSymbol(name);
}

}  // namespace wgsl