#pragma once
#include "base/string_utils.h"
#include "heap_allocator.h"
#include "slab_allocator_heap.h"

namespace gpu::d3d12 {

// Slab based general purpose resource allocator
// Allocates heaps from gpu and manually suballocates from them
// Heaps are split into pages and multiple pages grouped into spans
// Each span contains some number of slots of the same size
// After the span is freed it's pages are returned to the page free list,
//   coalesced if possible with neighbor spans and reused later
// Can contain only buffers and non-RT-DS textures
// TODO:
// - Dedicated allocations for objects larger than kMaxSlotSize (4MiB)
// - Allocation into a user provided heap
// - Make allocation with 0 offset invalid
class SlabAllocator {
public:
    // 'allowHeapAllocations' allows allocations of additional heaps when
    // no space in currently used heaps
    static std::unique_ptr<SlabAllocator> Create(
        HeapAllocator* heapAllocator,
        AllocationTracker* tracker = nullptr,
        uint32_t heapSize = internal::kDefaultHeapSize,
        bool allowHeapAllocations = true,
        const std::string& debugHeapNamePrefix = "SlabAllocatorHeap");

    ~SlabAllocator();

public:
    using AllocResult = std::expected<ResourceHeapAllocation, GenericErrorCode>;

    AllocResult Allocate(uint32_t size);
    void Free(const ResourceHeapAllocation& location);

    // Preallocates a heap using HeapAllocator for later usage
    void PreallocateHeap(int num);
    // Add a specific heap for allocation
    void SubmitHeap(RefCountedPtr<ID3D12Heap> heap) {
        DASSERT("Not implemented");
    }

private:
    using HeapMetadata = d3d12::internal::HeapMetadata;
    using Span = d3d12::internal::HeapMetadata::Span;
    using Bucket = d3d12::internal::Bucket;
    using FreeSpanCache = d3d12::internal::FreeSpanCache;

    SlabAllocator(const std::string prefix,
                  uint32_t size,
                  HeapAllocator* heapAlloc,
                  AllocationTracker* tracker);

    // Returns a span of entire heap
    Span* AllocateHeap();
    HeapMetadata* FindHeap(const ResourceHeapAllocation& location);

private:
    const std::string debugHeapNamePrefix_;
    const uint32_t heapSize_;

    AllocationTracker* const tracker_;
    HeapAllocator* const heapAllocator_;
    bool allowHeapsAllocations_;

    uint32_t numRetiredHeaps_ = 0;
    FreeSpanCache decommittedSpansCache_;
    // List of all heaps: active, empty, full
    HeapMetadata* heapHead_ = nullptr;
    // Ignore 0 bucket
    Bucket buckets_[internal::kNumBuckets + 1]{};
};



/*===========================================================================*/
inline std::unique_ptr<SlabAllocator> SlabAllocator::Create(
    HeapAllocator* heapAllocator,
    AllocationTracker* tracker,
    uint32_t heapSize,
    bool allowHeapsAllocations,
    const std::string& debugHeapNamePrefix) {
    //
    return std::unique_ptr<SlabAllocator>(new SlabAllocator(
        debugHeapNamePrefix, heapSize, heapAllocator, tracker));
}

inline SlabAllocator::SlabAllocator(const std::string prefix,
                                    uint32_t size,
                                    HeapAllocator* heapAlloc,
                                    AllocationTracker* tracker)
    : debugHeapNamePrefix_(prefix)
    , heapSize_(internal::ValidateHeapSize(size))
    , tracker_(tracker)
    , heapAllocator_(heapAlloc)
    , decommittedSpansCache_(heapSize_ / internal::kPageSize) {
    for (int i = 1; i < (internal::kNumBuckets + 1); ++i) {
        buckets_[i].Init(i);
    }
}

inline SlabAllocator::~SlabAllocator() {
    // Assert that all heaps are empty
    for (HeapMetadata* heap : ListIterate(heapHead_)) {
        DASSERT(heap->IsEmpty());
        Span* rootSpan = heap->GetRootSpan();
        // For assert inside the span destructor
        decommittedSpansCache_.Remove(rootSpan);
    }
    ListDelete(heapHead_);
}

inline void SlabAllocator::PreallocateHeap(int num) {
    DASSERT_F(allowHeapsAllocations_,
              "Cannot allocate new heaps if 'allowHeapsAllocations' is false");
    for (int i = 0; i < num; ++i) {
        Span* span = AllocateHeap();
        decommittedSpansCache_.Push(span);
    }
}

inline SlabAllocator::AllocResult SlabAllocator::Allocate(uint32_t size) {
    DASSERT(size <= internal::kMaxSlotSize);
    const uint32_t requestedSize = size;
    DASSERT(requestedSize < internal::kDefaultHeapSize);
    const uint32_t slotSize = AlignUp(requestedSize, internal::kMinAlignment);
    // Every bucket size is multiple of 'kMinBucketSize'
    const uint32_t sizeClass = slotSize >> internal::kMinSlotSizeShift;
    Bucket& bucket = buckets_[sizeClass];

    Span* span = [&] {
        // Fast path:
        // - Allocate directly from an active span in a bucket
        Span* span = bucket.GetActiveSpan();
        if (span) {
            return span;
        }
        // Slow path:
        // - Take the first-fit span from the free span cache
        // - Split the span if needed
        const uint32_t numPagesRequired = bucket.GetSpanSizeInPages();
        span = decommittedSpansCache_.PopFirstFit(numPagesRequired);

        if (span) {
            // Split the span if larger
            if (span->GetNumPages() > numPagesRequired) {
                Span* right = span->Split(numPagesRequired);
                decommittedSpansCache_.Push(right);
            }
            span->Commit(slotSize);
            buckets_[sizeClass].EmplaceSpan(span);
            return span;
        }
        // Allocate a new heap
        if (allowHeapsAllocations_) {
            span = AllocateHeap();
            DASSERT(span);
            DASSERT(span->GetNumPages() >= numPagesRequired);
            if (span->GetNumPages() > numPagesRequired) {
                Span* right = span->Split(numPagesRequired);
                decommittedSpansCache_.Push(right);
            }
            span->Commit(slotSize);
            buckets_[sizeClass].EmplaceSpan(span);
            return span;
        }
        return (Span*)nullptr;
    }();
    if (!span) {
        // Cannot allocate. Report error
        return std::unexpected(GenericErrorCode::OutOfMemory);
    }
    const uint32_t heapOffset = span->AllocateSlot();
    return ResourceHeapAllocation{.heap = span->GetHeap()->GetHeap(),
                             .offset = heapOffset};
}

inline void SlabAllocator::Free(const ResourceHeapAllocation& location) {
    HeapMetadata* heap = FindHeap(location);
    DASSERT(heap);
    Span* span = heap->GetSpanFromByteOffset(location.offset);
    DASSERT(span);
    span->FreeSlot(location.offset);

    if (span->IsEmpty()) {
        const uint32_t bucket = span->GetSizeClass();
        buckets_[bucket].RemoveSpan(span);
        span->Decommit();
        // Eager coalescing
        Span* left = span->GetLeftNeighbour();
        if (left && left->IsDecommitted()) {
            decommittedSpansCache_.Remove(left);
            span = span->MergeLeft(left);
        }
        Span* right = span->GetRightNeighbour();
        if (right && right->IsDecommitted()) {
            decommittedSpansCache_.Remove(right);
            span = span->MergeRight(right);
        }
        decommittedSpansCache_.Push(span);
    }
    if (heap->IsEmpty()) {
        ++numRetiredHeaps_;
        // If too many heaps are empty, release the heap to the system
        if (numRetiredHeaps_ > internal::kMaxNumRetiredHeaps) {
            decommittedSpansCache_.Remove(span);
            DListRemove(heapHead_, heap);
            delete heap;
        }
    }
}

// Returns a span of entire heap
inline internal::Span* SlabAllocator::AllocateHeap() {
    DASSERT(allowHeapsAllocations_);
    DASSERT(heapAllocator_);
    HeapAllocator::AllocResult result = heapAllocator_->Allocate(heapSize_);
    DASSERT_F(result, "Cannot allocate new heap. {}",
              to_string(result.error()));
    RefCountedPtr<ID3D12Heap> d3d12Heap = std::move(*result);

    const std::string heapName =
        std::format("{} Heap {}", debugHeapNamePrefix_, ListSize(heapHead_));

    if (kDebugBuild) {
        const std::wstring heapNameWide = ToWideString(heapName);
        d3d12Heap->SetName(heapNameWide.c_str());
    }
    auto* heap = new HeapMetadata(std::move(d3d12Heap), heapName, tracker_);
    DListPush(heapHead_, heap);
    Span* initSpan = heap->GetRootSpan();
    return initSpan;
}

inline internal::HeapMetadata* SlabAllocator::FindHeap(
    const ResourceHeapAllocation& location) {
    for (HeapMetadata* heap : ListIterate(heapHead_)) {
        if (heap->GetHeap() == location.heap) {
            return heap;
        }
    }
    return nullptr;
}

}  // namespace gpu::d3d12
