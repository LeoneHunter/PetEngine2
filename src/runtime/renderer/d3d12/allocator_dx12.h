#include "allocator_dx12_heap.h"

namespace dx12 {

// Slab based general purpose resource allocator
// Allocates heaps from gpu and manually suballocates from them
// Heaps are split into pages and multiple pages grouped into spans
// Each span contains some number of slots of the same size
// After the span is freed it's pages are returned to the page free list, 
//   coalesced if possible with neighbor spans and reused later
// Can contain only buffers and non-RT-DS textures
// TODO:
// - Extract heap managing into a separate class HeapPool
//   So that user can provide a specific pool or heap for allocation
// - Dedicated allocations for objects larger than kMaxSlotSize (4MiB)
// - Allocation into a user provided heap
// - Make 0 allocation invalid
class ResourceSlabAllocator {
public:

    // Result of the allocation
    // A heap and an offset into it in bytes
    // Can be used then: heap->CreatePlacedResource(offset)
    struct Allocation {
        ID3D12Heap* heap = nullptr;
        uint32_t offset = 0;
    };

public:
    ResourceSlabAllocator(dx12::Device* device,
                          AllocationTracker* tracker = nullptr);

    ~ResourceSlabAllocator();

public:
    std::expected<Allocation, Error> Allocate(const D3D12_RESOURCE_DESC& desc);
    void Free(const Allocation& location);

    // Should be used only for testing
    // Because different hardware supports different resources in differnt heaps
    void PreallocateHeapsForTesting(HeapResourceType type, int num);

private:
    using Span = internal::Span;
    using HeapMetadata = internal::HeapMetadata;
    using Bucket = internal::Bucket;
    using FreeSpanCache = internal::FreeSpanCache;

    // Returns a span of entire heap
    Span* AllocateHeap(HeapResourceType type);

    constexpr HeapMetadata* FindHeap(const Allocation& location);

private:
    struct HeapLists {
        // FIXME: Actually we only need a single list
        // All allocation managing is done through span caches
        HeapMetadata* activeHeapHead_;
        HeapMetadata* fullHeapHead_;
        HeapMetadata* retiredHeapHead_;
    };
    HeapLists heaps_[(int)HeapResourceType::_Count]{};
    // Ignore 0 bucket
    Bucket buckets_[internal::kNumBuckets + 1]{};
    FreeSpanCache decommittedSpansCache_;

    Device* const device_;
    uint32_t numHeaps_ = 0;

    AllocationTracker* const tracker_;
};



/*===========================================================================*/
ResourceSlabAllocator::ResourceSlabAllocator(dx12::Device* device,
                                             AllocationTracker* tracker)
    : device_(device), tracker_(tracker) {
    for(int i = 1; i < (internal::kNumBuckets + 1); ++i) {
        buckets_[i].Init(i);
    }
}

ResourceSlabAllocator::~ResourceSlabAllocator() {
    for(HeapLists& heaps: heaps_) {
        ListDelete(heaps.activeHeapHead_);
        ListDelete(heaps.fullHeapHead_);
        ListDelete(heaps.retiredHeapHead_);
    }
}

void ResourceSlabAllocator::PreallocateHeapsForTesting(HeapResourceType type,
                                             int num) {
    for(int i = 0; i < num; ++i) {
        Span* span = AllocateHeap(type);
        decommittedSpansCache_.Push(span);
    }
}

std::expected<ResourceSlabAllocator::Allocation, Error>
ResourceSlabAllocator::Allocate(const D3D12_RESOURCE_DESC& desc) {
    DASSERT(desc.Width <= internal::kMaxSlotSize);
    const uint32_t requestedSize = (uint32_t)desc.Width * desc.Height;
    DASSERT(requestedSize < internal::kHeapSize);
    const uint32_t slotSize =
        AlignUp(requestedSize, (uint32_t)internal::kMinAlignment);
    // Every bucket size is multiple of 'kMinBucketSize'
    const uint32_t sizeClass = slotSize >> internal::kMinSlotSizeShift;
    Bucket& bucket = buckets_[sizeClass];
    
    Span* span = [&]{
        // Fast path:
        // - Allocate directly from an active span in a bucket
        Span* span = bucket.GetActiveSpan();
        if(span) {
            return span;
        }
        // Slow path:
        // - Take the first-fit span from the free span cache
        // - Split the span if needed
        const uint32_t numPagesRequired = bucket.GetSpanSizeInPages();
        span = decommittedSpansCache_.PopFirstFit(numPagesRequired);

        if(span) {
            // Split the span if larger
            if(span->GetNumPages() > numPagesRequired) {
                Span* right = span->Split(numPagesRequired);
                decommittedSpansCache_.Push(right);
            }
            span->Commit(slotSize);
            buckets_[sizeClass].EmplaceSpan(span);
            return span;
        }
        // Slowest path: Allocate new heap
        HeapResourceType heapType;
        D3D12_RESOURCE_HEAP_TIER maxHeapTierSupported =
            device_->GetProperties().resourceHeapTier;
        // For heap tier 1 we should use different heaps for textures, 
        //     render targets and buffers
        // For heap tier 2 and higher we can use one heap for all types
        if(maxHeapTierSupported < D3D12_RESOURCE_HEAP_TIER_2) {
            switch(desc.Dimension) {
                case D3D12_RESOURCE_DIMENSION_BUFFER: 
                    heapType = HeapResourceType::Buffer;
                    break;
                case D3D12_RESOURCE_DIMENSION_TEXTURE1D: 
                case D3D12_RESOURCE_DIMENSION_TEXTURE2D: 
                case D3D12_RESOURCE_DIMENSION_TEXTURE3D: 
                    heapType = HeapResourceType::NonRtDsTexture;
                    break;
                default: 
                    heapType = HeapResourceType::Unknown;
            }
        } else {
            heapType = HeapResourceType::Unknown;
        }
        // Check if have retired heap
        HeapLists& heaps = heaps_[(int)heapType];
        HeapMetadata* heap = heaps.retiredHeapHead_;

        if(heap) {
            DListPop(heaps.retiredHeapHead_);
            DListPush(heaps.activeHeapHead_, heap);
            span = heap->CreateInitSpan();
        } else {
            // Allocate new
            span = AllocateHeap(heapType);
            DASSERT(span);
            DASSERT(span->GetNumPages() > numPagesRequired);
        }

        Span* right = span->Split(numPagesRequired);
        decommittedSpansCache_.Push(right);
        span->Commit(slotSize);
        buckets_[sizeClass].EmplaceSpan(span);
        return span;
    }();
    if(!span) {
        // Cannot allocate. Report error
        return std::unexpected(Error::CannotAllocate);
    }
    const uint32_t heapOffset = span->AllocateSlot();
    return Allocation{
        .heap = span->GetHeap()->GetHeap(),
        .offset = heapOffset
    };
}

void ResourceSlabAllocator::Free(const Allocation& location) {
    HeapMetadata* heap = FindHeap(location);
    DASSERT(heap);
    Span* span = heap->GetSpanFromByteOffset(location.offset);
    DASSERT(span);
    span->FreeSlot(location.offset);

    if(span->IsEmpty()) {
        const uint32_t bucket = span->GetSizeClass();
        buckets_[bucket].RemoveSpan(span);
        span->Decommit();
        // Eager coalescing
        Span* left = span->GetLeftNeighbour();
        if(left && left->IsDecommitted()) {
            decommittedSpansCache_.Remove(left);
            span = span->MergeLeft(left);
        }
        Span* right = span->GetRightNeighbour();
        if(right && right->IsDecommitted()) {
            decommittedSpansCache_.Remove(right);
            span = span->MergeRight(right);
        }
        decommittedSpansCache_.Push(span);
    }
    if(heap->IsEmpty()) {
        HeapLists& heaps = heaps_[(int)heap->GetType()];
        DASSERT(ListContains(heaps.activeHeapHead_, heap));
        DListRemove(heaps.activeHeapHead_, heap);
        decommittedSpansCache_.Remove(span);
        const size_t numRetiredHeaps = ListSize(heaps.retiredHeapHead_);
        // If too many heaps are empty, release the heap to the system
        if(numRetiredHeaps > internal::kMaxNumRetiredHeaps) {
            delete heap;
        } else {
            DListPush(heaps.retiredHeapHead_, heap);
        }
    }
}

// Returns a span of entire heap
internal::Span* ResourceSlabAllocator::AllocateHeap(HeapResourceType type) {
    DASSERT(type < HeapResourceType::_Count);
    // For general purpose allocation only default is needed
    // Upload heaps should be managed by a different allocator
    CD3DX12_HEAP_DESC heapDesc{
        internal::kHeapSize,
        D3D12_HEAP_TYPE_DEFAULT,
        internal::kHeapAlignment,
    };
    // Create a debug heap name
    std::string heapNamePrefix;
    // For heap tier 1 we should use different heaps for textures, render
    // targets and buffers For heap tier 2 and higher we can use one heap for
    // all types
    switch (type) {
        case HeapResourceType::Buffer:
            heapDesc.Flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            heapNamePrefix = "Buffers";
            break;
        case HeapResourceType::NonRtDsTexture:
            heapDesc.Flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
            heapNamePrefix = "Textures";
            break;
        case HeapResourceType::Unknown:
            heapDesc.Flags |= D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
            heapNamePrefix = "Textures & Buffers";
            break;
    }
    RefCountedPtr<ID3D12Heap> d3d12Heap;
    auto hr =
        device_->GetDevice()->CreateHeap(&heapDesc, IID_PPV_ARGS(&d3d12Heap));
    DASSERT_HR(hr);

    ++numHeaps_;
    const std::string heapName =
        std::format("{} Resource Heap {}", heapNamePrefix, numHeaps_);

    if (kDebugBuild) {
        const std::wstring heapNameWide = util::ToWideString(heapName);
        d3d12Heap->SetName(heapNameWide.c_str());
    }
    auto* heap =
        new HeapMetadata(type, std::move(d3d12Heap), heapName, tracker_);

    HeapLists& heaps = heaps_[(int)type];
    DListPush(heaps.activeHeapHead_, heap);

    Span* initSpan = heap->CreateInitSpan();
    return initSpan;
}

constexpr internal::HeapMetadata* ResourceSlabAllocator::FindHeap(
        const Allocation& location) {
    for (HeapLists& heapsPerType : heaps_) {
        for (HeapMetadata* heap : ListIterate(heapsPerType.activeHeapHead_)) {
            if (heap->GetHeap() == location.heap) {
                return heap;
            }
        }
        for (HeapMetadata* heap : ListIterate(heapsPerType.fullHeapHead_)) {
            if (heap->GetHeap() == location.heap) {
                return heap;
            }
        }
    }
    return nullptr;
}

} // namespace dx12
