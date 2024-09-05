#include "program.h"
#include "lexer.h"
#include "parser.h"
#include "program_builder.h"

#include "base/tree_printer.h"
#include "ast_printer.h"

namespace wgsl {

std::unique_ptr<Program> Program::Create(std::string_view code) {
    auto builder = ProgramBuilder();
    builder.Build(code);
    auto program = builder.Finalize();
    return program;
}

std::string Program::GetDiagsAsString() {
    std::string out;
    for (uint32_t i = 0; i < diags_.size(); ++i) {
        const auto& msg = diags_[i];
        const auto type = msg.type == DiagMsg::Type::Error ? "error" : "warning";
        const auto loc = msg.loc.start;
        out += std::format("\n{} [Ln {}, Col {}]: {}", type, loc.line, loc.col,
                           msg.msg);
    }
    return out;
}

void Program::PrintAst(TreePrinter* printer) const {
    ast::AstPrinter astPrinter;
    for(const ast::Variable* var: globalDecls_) {
        astPrinter.Print(printer, var);
    }
}

}  // namespace wgsl