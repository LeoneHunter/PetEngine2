#include "program.h"
#include <doctest.h>

using namespace wgsl;

TEST_CASE("[wgsl::Parser] Basic") {
    auto program = Program::Create(R"(
        const a = 2 * 4;
        const b = 6f;
        const c : f32 = 45;
    )");
    CHECK_EQ(program->globalDecls_.size(), 3);
}