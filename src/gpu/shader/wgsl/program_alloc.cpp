#include "common.h"
#include "program_alloc.h"
#include "program.h"

namespace wgsl::internal {

void* ProgramAllocBase::AllocateImpl(size_t size, size_t align) {
    auto* program = Program::GetCurrent();
    DASSERT_F(program, "ProgramAlloc can only be used with a program");
    auto* mem = program->alloc_.Allocate(size, align);
    DASSERT(mem);
    return mem;
}

}

