#include "base/util.h"
#include "descriptor_heap.h"
#include "device.h"

#include <map>

#include <doctest/doctest.h>

using namespace gpu::d3d12;

namespace {

struct DX12Test {

    DX12Test() {
        const int currentAdapter = 0;
        device = TRY(DeviceFactory::CreateDevice(currentAdapter, kDebugBuild));
    }

    RefCountedPtr<DeviceD3D12> device;
};

struct DescriptorHeapTest : public DX12Test {

    DescriptorHeapTest() {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 5,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        };
        heap = std::make_unique<DescriptorHeap>(device.Get(), desc,
                                                "DescriptorHeapTest");
        CHECK(heap->HasProperties(desc));
        CHECK_EQ(heap->GetFreeSize(), 5);
    }

    std::unique_ptr<DescriptorHeap> heap;
};

}  // namespace

TEST_CASE_FIXTURE(DescriptorHeapTest, "[DescriptorHeap] Basic") {
    std::optional<DescriptorLocation> alloc0 = heap->TryAllocate(2);
    // x x - - -
    CHECK(alloc0);
    CHECK_EQ(heap->GetFreeSize(), 3);

    std::optional<DescriptorLocation> alloc1 = heap->TryAllocate(2);
    // x x x x -
    CHECK(alloc1);
    CHECK_EQ(heap->GetFreeSize(), 1);

    std::optional<DescriptorLocation> alloc2 = heap->TryAllocate(1);
    // x x x x x
    CHECK(alloc2);
    CHECK_EQ(heap->GetFreeSize(), 0);

    std::optional<DescriptorLocation> alloc3 = heap->TryAllocate(1);
    // x x x x x
    CHECK(!alloc3);
    CHECK_EQ(heap->GetFreeSize(), 0);

    heap->Free(*alloc1);
    // x x - - x
    CHECK_EQ(heap->GetFreeSize(), 2);

    heap->Free(*alloc2);
    // x x - - -
    CHECK_EQ(heap->GetFreeSize(), 3);

    std::optional<DescriptorLocation> alloc4 = heap->TryAllocate(3);
    // x x x x x
    CHECK(alloc4);
    CHECK_EQ(heap->GetFreeSize(), 0);

    heap->Free(*alloc0);
    // - - x x x
    CHECK_EQ(heap->GetFreeSize(), 2);

    heap->Free(*alloc4);
    // - - - - -
    CHECK_EQ(heap->GetFreeSize(), 5);
}

TEST_CASE_FIXTURE(DescriptorHeapTest, "[DescriptorHeap] Double merge") {
    std::optional<DescriptorLocation> alloc0 = heap->TryAllocate(2);
    std::optional<DescriptorLocation> alloc1 = heap->TryAllocate(2);
    std::optional<DescriptorLocation> alloc2 = heap->TryAllocate(1);

    heap->Free(*alloc0);
    heap->Free(*alloc2);
    // - - x x -
    CHECK_EQ(heap->GetFreeSize(), 3);

    std::optional<DescriptorLocation> alloc3 = heap->TryAllocate(3);
    CHECK(!alloc3);

    heap->Free(*alloc1);
    // - - - - -
    CHECK_EQ(heap->GetFreeSize(), 5);

    std::optional<DescriptorLocation> alloc4 = heap->TryAllocate(5);
    // x x x x x
    CHECK(alloc4);
    CHECK_EQ(heap->GetFreeSize(), 0);
}