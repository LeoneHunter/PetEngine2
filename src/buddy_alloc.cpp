#include "base/buddy_alloc.h"

int Test1() {
    auto allocs = std::array<uint32_t, 4>();
    // [------------------- 16 -----------------]
    // [--------- 8 --------][------- 8 --------]
    // [--- 4 ---][--- 4 ---][-- 4 ---][-- 4 ---]
    auto alloc = BuddyAlloc(16, 4);
    // [------------------ 16 ------------------]
    allocs[0] = alloc.Allocate(3);
    // [xxx 4 xxx][--- 4 ---][-------- 8 -------]
    allocs[1] = alloc.Allocate(3);
    // [xxx 4 xxx][xxx 4 xxx][-------- 8 -------]
    allocs[2] = alloc.Allocate(3);
    // [xxx 4 xxx][xxx 4 xxx][xxx 4 xxx][--- 4 ---]
    allocs[3] = alloc.Allocate(3);
    // [xxx 4 xxx][xxx 4 xxx][xxx 4 xxx][xxx 4 xxx]
    auto oom = alloc.Allocate(3);
    DASSERT(oom == BuddyAlloc::kOutOfMemoryOffset);

    alloc.Free(allocs[3]);
    // [xxx 4 xxx][xxx 4 xxx][xxx 4 xxx][--- 4 ---]
    alloc.Free(allocs[0]);
    // [--- 4 ---][xxx 4 xxx][xxx 4 xxx][--- 4 ---]
    alloc.Free(allocs[2]);
    // [--- 4 ---][xxx 4 xxx][-------- 8 -------]
    alloc.Free(allocs[1]);
    // [------------------ 16 ------------------]

    return 0;
}

int Test2() {
    auto allocs = std::array<uint32_t, 4>();
    // [------------------- 16 -----------------]
    // [--------- 8 --------][------- 8 --------]
    // [--- 4 ---][--- 4 ---][-- 4 ---][-- 4 ---]
    auto alloc = BuddyAlloc(16, 4);
    allocs[0] = alloc.Allocate(3);
    allocs[1] = alloc.Allocate(3);
    allocs[2] = alloc.Allocate(3);
    allocs[3] = alloc.Allocate(3);
    auto oom = alloc.Allocate(3);
    DASSERT(oom == BuddyAlloc::kOutOfMemoryOffset);

    alloc.Free(allocs[2]);
    alloc.Free(allocs[0]);
    alloc.Free(allocs[1]);
    alloc.Free(allocs[3]);

    return 0;
}

int Test3() {
    auto allocs = std::array<uint32_t, 8>();
    auto alloc = BuddyAlloc(16, 2);
    for (int i = 0; i < 8; ++i) {
        allocs[i] = alloc.Allocate(2);
    }
    for (int i = 0; i < 8; ++i) {
        alloc.Free(allocs[i]);
    }
    return 0;
}

int Test4() {
    auto allocs = std::vector<uint32_t>();
    auto alloc = BuddyAlloc(65536, 256);

    constexpr int numAllocs = 20;
    std::mt19937 generator;
    std::uniform_int_distribution<uint32_t> dist(256, 2048);

    for (int i = 0; i < numAllocs; ++i) {
        const uint32_t size = dist(generator);
        allocs.push_back(alloc.Allocate(size));
    }
    for (int i = 0; i < numAllocs; ++i) {
        alloc.Free(allocs[i]);
    }
    return 0;
}

int main(int argc, char** argv) {
    Test1();
    Test2();
    Test3();
    Test4();
    return 0;
}