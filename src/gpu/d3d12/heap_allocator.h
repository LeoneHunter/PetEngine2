#pragma once
#include "common.h"
#include "device.h"

#include <expected>

namespace gpu::d3d12 {

// Allocates heaps with specific properties
// Provides upstream heap resources to allocators
// TODO: Add high level memory tracking and validation for oom
class HeapAllocator {
public:
    static std::unique_ptr<HeapAllocator> Create(
        DeviceD3D12* device,
        D3D12_HEAP_TYPE type,
        D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) {
        //
        auto alloc = std::unique_ptr<HeapAllocator>(new HeapAllocator(device));
        alloc->type_ = type;
        alloc->flags_ = flags;
        return alloc;
    }

    using AllocResult =
        std::expected<RefCountedPtr<ID3D12Heap>, GenericErrorCode>;

    AllocResult Allocate(size_t size) {
        D3D12_HEAP_DESC heapDesc{};
        heapDesc.SizeInBytes = size;
        heapDesc.Properties.Type = type_;
        heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapDesc.Properties.CreationNodeMask = 0;
        heapDesc.Properties.VisibleNodeMask = 0;
        heapDesc.Alignment = 0;
        heapDesc.Flags = flags_;

        RefCountedPtr<ID3D12Heap> d3d12Heap;
        auto hr = device_->GetNative()->CreateHeap(
            &heapDesc, IID_PPV_ARGS(&d3d12Heap));
        DASSERT_HR(hr);
        return std::move(d3d12Heap);
    }

private:
    HeapAllocator(DeviceD3D12* device) : device_(device) {}

private:
    RefCountedPtr<DeviceD3D12> device_;
    D3D12_HEAP_TYPE type_;
    D3D12_HEAP_FLAGS flags_;
};

}  // namespace d3d12