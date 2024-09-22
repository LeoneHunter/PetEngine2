#pragma once
#include "common.h"

#include <list>
#include <forward_list>

namespace wgsl {

namespace internal {
struct ProgramAllocBase {
    static void* AllocateImpl(size_t size, size_t align);
};
} // namespace internal

// allocates memory using program allocator so that the objects lifetime is
// bound to the lifetime of a program all memory is freed at once on program
// destruction note: destructors are not called
template <class T>
struct ProgramAlloc : public internal::ProgramAllocBase {
    using value_type = T;

    constexpr ProgramAlloc() = default;

    template <class U>
    constexpr ProgramAlloc(const ProgramAlloc<U>&) {}

    [[nodiscard]] T* allocate(std::size_t n) {
        return static_cast<T*>(AllocateImpl(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n) { }
};

// allocated by a program allocator and owned by a program
// note: destructors are not called
template <class T>
using ProgramList = std::list<T, ProgramAlloc<T>>;

// allocated by a program allocator and owned by a program
// note: destructors are not called
template <class T>
using ProgramVector = std::vector<T, ProgramAlloc<T>>;


} // namespace wgsl
