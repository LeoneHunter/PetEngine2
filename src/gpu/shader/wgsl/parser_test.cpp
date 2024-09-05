#include "program.h"
#include <doctest.h>

#include "base/tree_printer.h"

using namespace wgsl;

TEST_CASE("[wgsl::Parser] Basic") {
    auto program = Program::Create(R"(
        const c = 3;
    )");
    JsonTreePrinter printer;
    program->PrintAst(&printer);
    Println("[wgsl] Program ast: \n{}", printer.Finalize());
    const std::string diag = program->GetDiagsAsString();
    if(!diag.empty()) {
        Println("[wgsl] Parse diagnostics: \"{}\"", diag);
    }
    // CHECK_EQ(program->globalDecls_.size(), 1);
}