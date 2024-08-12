#include "common_dx12.h"
#include "device_dx12.h"

#include <expected>

namespace d3d12 {

// Allocates heaps with specific properties
// Provides upstream heap resources to allocators
// TODO: Add high level memory tracking and validation for oom
class HeapAllocator {
public:
    HeapAllocator(d3d12::Device* device,
                  D3D12_HEAP_TYPE type,
                  D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE)
        : device_(device)
        , type_(type)
        , flags_(flags) 
    {}

    using AllocResult = std::expected<RefCountedPtr<ID3D12Heap>, Error>;

    AllocResult Create(size_t size) {
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
        auto hr = device_->GetDevice()->CreateHeap(&heapDesc, IID_PPV_ARGS(&d3d12Heap));
        DASSERT_HR(hr);
        return std::move(d3d12Heap);
    }

private:
    d3d12::Device* device_;
    D3D12_HEAP_TYPE type_;
    D3D12_HEAP_FLAGS flags_;
};

} // namespace d3d12