#pragma once
#include "device_dx12.h"

namespace dx12 {

class PooledDescriptorAllocator;

struct DescriptorLocation {
    D3D12_CPU_DESCRIPTOR_HANDLE ptrCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE ptrGpu;
};

// Wrapper around ID3D12DescriptorHeap with simple management of allocated
// segments
class DescriptorHeap {
public:

    DescriptorHeap(ID3D12Device* device, 
                   D3D12_DESCRIPTOR_HEAP_DESC desc,
                   const std::string& debugName);

    // Return null if there's not enough space
    std::optional<DescriptorLocation> TryAllocate(size_t size);

    void Free(DescriptorLocation location);

    // Checks whether the descriptor at 'location' is inside the heap
    bool ContainsLocation(DescriptorLocation location) const;

    const D3D12_DESCRIPTOR_HEAP_DESC& GetDesc() const { return desc_; }

    bool HasProperties(D3D12_DESCRIPTOR_HEAP_DESC desc) {
        return desc_.Flags == desc.Flags && desc_.Type == desc.Type;
    }

    size_t GetFreeSize() const { return freeSize_; }

    // Get total allocated size
    size_t GetCapacity() const { return desc_.NumDescriptors; }

    const std::string GetDebugName() const { return debugName_; }

private:
    // Info about a single allocation
    struct SegmentMetadata {
        size_t startSlotIndex;
        size_t size: 63;
        size_t isFree: 1;
    };

    DescriptorLocation GetLocationForIndex(size_t index) const {
        const size_t ptrOffset = index * descriptorSize_;
        return DescriptorLocation{
            .ptrCpu = baseCpu_.ptr + ptrOffset,
            .ptrGpu = baseGpu_.ptr + ptrOffset,
        };
    }

private:
    std::string                         debugName_;
    RefCountedPtr<ID3D12DescriptorHeap> d3dHeap_;
    D3D12_DESCRIPTOR_HEAP_DESC          desc_;
    // All allocated segments
    std::vector<SegmentMetadata>        segments_;
    D3D12_CPU_DESCRIPTOR_HANDLE         baseCpu_;
    D3D12_GPU_DESCRIPTOR_HANDLE         baseGpu_;
    size_t                              descriptorSize_;
    size_t                              freeSize_;
};

// Allocates descriptors from the internally managed pool of 
// descriptor heaps of different types
class PooledDescriptorAllocator {
public:

    static std::unique_ptr<PooledDescriptorAllocator> Create(
            Device* parentDevice,
            const std::string& debugName);

    // Allocates a heap of 'type' with 'size' in advance
    DescriptorHeap& CreateHeap(D3D12_DESCRIPTOR_HEAP_DESC desc,
                               const std::string& debugName = "");

    size_t GetDescriptorIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type);

    // Returns the location of the first free descriptor
    // Allocator owns the underlying storage
    DescriptorLocation Allocate(const D3D12_DESCRIPTOR_HEAP_DESC& desc);

    // Frees the range starting from location
    void Free(DescriptorLocation location);
    
public: 
    ~PooledDescriptorAllocator() = default;

    PooledDescriptorAllocator(const PooledDescriptorAllocator&) = delete;
    PooledDescriptorAllocator& operator=(const PooledDescriptorAllocator&) = delete;

    PooledDescriptorAllocator(PooledDescriptorAllocator&&) = delete;
    PooledDescriptorAllocator& operator=(PooledDescriptorAllocator&&) = delete;

protected:
    PooledDescriptorAllocator() = default;

private:
    Device* device_{};
    std::string debugName_;
    std::vector<std::unique_ptr<DescriptorHeap>> heaps_;
};

} // namespace dx12
