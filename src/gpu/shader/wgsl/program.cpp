#include "program.h"
#include "lexer.h"
#include "parser.h"
#include "program_builder.h"

#include "ast_printer.h"
#include "base/tree_printer.h"

namespace wgsl {

std::unique_ptr<Program> Program::Create(std::string_view code) {
    auto builder = ProgramBuilder();
    builder.Build(code);
    auto program = builder.Finalize();
    return program;
}

Program::Program() {
    globalScope_ = alloc_.Allocate<Scope>(nullptr);
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

const std::vector<Program::DiagMsg>& Program::GetDiags() {
    return diags_;
}

void Program::PrintAst(TreePrinter* printer) const {
    ast::AstPrinter astPrinter;
    for (const ast::Variable* var : globalScope_->variables) {
        astPrinter.Print(printer, var);
    }
}

const ast::Node* Program::FindSymbol(std::string_view name,
                                     std::string_view scope) const {
    DASSERT(scope.empty() && "Unimplemented");
    return globalScope_->FindSymbol(name);
}

void Program::CreateBuiltinTypes() {
    auto start = (uint64_t)Type::Kind::AbstrInt;
    auto end = (uint64_t)Type::Kind::F16;
    for (uint32_t i = start; i <= end; ++i) {
        const auto kind = (Type::Kind)i;
        auto* type =
            alloc_.Allocate<ast::Type>(SourceLoc(), kind, to_string(kind));
        globalScope_->InsertSymbol(to_string(kind), type);
    }
}

}  // namespace wgsl