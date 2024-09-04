#include "program.h"
#include "lexer.h"
#include "parser.h"
#include "program_builder.h"

namespace wgsl {

std::unique_ptr<Program> Program::Create(std::string_view code) {
    auto builder = ProgramBuilder();
    auto parser = Parser(code, &builder);
    parser.Parse();
    return builder.Finalize();
}

}  // namespace wgsl