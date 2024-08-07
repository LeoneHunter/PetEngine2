#pragma once
#include "device_dx12.h"
#include "runtime/util.h"

#include <bitset>
#include <expected>
#include <variant>

namespace d3d12 {
namespace internal {

// 64 KiB
// D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
constexpr uint32_t kMinAlignmentShift = 16;
constexpr uint32_t kMinAlignment = 1 << kMinAlignmentShift;

// 4 MiB
// D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
constexpr uint32_t kMaxAlignmentShift = 22;
constexpr uint32_t kMaxAlignment = 1 << kMaxAlignmentShift;

// 64 KiB == kMinAlignment
constexpr uint32_t kMinSlotSizeShift = kMinAlignmentShift;
constexpr uint32_t kMinSlotSize = 1 << kMinSlotSizeShift;
// 4 MiB
constexpr uint32_t kMaxSlotSizeShift = kMaxAlignmentShift;
constexpr uint32_t kMaxSlotSize = 1 << kMaxSlotSizeShift;
constexpr uint32_t kMaxNumSlotsPerSpan = 64;

// 64 buckets at the 64KiB granularity
// Ignore 0 bucket. From 1 to 64
constexpr uint32_t kNumBuckets = kMaxSlotSize / kMinSlotSize;

// 2 MiB
// One page can contain 32 objects of the smallest size class
// And one object of the largest class requires 2 pages
constexpr uint32_t kPageSize = kMinSlotSize * 32;

constexpr uint32_t kNumPagesPerHeap = 128;
// 256 MiB
constexpr uint32_t kDefaultHeapSize = kPageSize * kNumPagesPerHeap;
constexpr uint32_t kHeapAlignment = kMaxAlignment;

// 8 objects in a span for large resources
constexpr uint32_t kMinSpanSize = 8;
constexpr uint32_t kMaxSpanSize = kPageSize / kMinSlotSize;

// 32 MiB
constexpr uint32_t kMinHeapSize = kMinSpanSize * kMaxSlotSize;
// 2 GiB
constexpr uint32_t kMaxHeapSize = std::numeric_limits<int32_t>::max();


// Maximum pages per span
// For a span of the largest bucket objects
// 16
constexpr uint32_t kMaxNumPagesPerSpan =
    (kMinSpanSize * kMaxSlotSize) / kPageSize;

constexpr uint32_t kMiB = 1024 * 1024;
constexpr uint32_t kGiB = 1024 * 1024 * 1024;
constexpr uint32_t kDefaultMaxNumSpansPerGiB =
    (kGiB / kDefaultHeapSize) * kNumPagesPerHeap;

// 8 KiB
constexpr uint32_t kHeapMetadataAlignment = 1 << 13;

// 1GiB of space
constexpr uint32_t kMaxNumRetiredHeaps = 4;

constexpr uint32_t SizeClassToSlotSize(uint32_t sizeClass) {
    return sizeClass * kMinSlotSize;
}

constexpr uint32_t ValidateHeapSize(uint32_t size) {
    size = std::clamp(size, kMinHeapSize, kMaxHeapSize);
    return AlignUp(size, kPageSize);
}

}  // namespace internal

namespace internal {

class HeapMetadata: public DListNode<HeapMetadata> {
public:
    // A contiguous span (slab) of pages carved up into slots
    // All allocations here are of the same size class
    // Basically a pool of object of 'slotSize' bytes
    // Can be in the one of four states:
    // - Decommitted: Empty span without size class
    //   Pages of such span can be reused and such
    //   spans can be coalesced to form a larger span
    // - Active: Has some allocated slots
    // - Full: All slots are allocated
    // - Empty: All slots are empty
    class Span: public DListNode<Span> {
    public:
        constexpr Span(HeapMetadata* heap)
            : heap_(heap) {
            Clear();
        }

        constexpr ~Span() { 
            DASSERT_F(!this->next && !this->prev, "Span still in use!"); 
        }

        // Initialize as brand new erasing old state completely
        constexpr void Init(uint32_t numPages, uint32_t slotSize = 0) {
            DASSERT(IsDecommitted());
            DASSERT(slots_ == 0);
            DASSERT(numPages);
            // Clean previous state
            if (numPages_) {
                const uint32_t lastPageIndex = GetPageIndex() + numPages - 1;
                PageIndexToSpan(lastPageIndex)->Clear();
                Clear();
            }
            numPages_ = numPages;
            if (slotSize) {
                Commit(slotSize);
            }
            if (numPages > 1) {
                const uint32_t lastPageIndex = GetPageIndex() + numPages - 1;
                DASSERT(lastPageIndex < GetHeap()->numPages_);
                Span* lastPageMetadata = PageIndexToSpan(lastPageIndex);
                lastPageMetadata->InitAsLast(numPages);
            }
        }

        constexpr void Clear() {
            slotSize_ = 0;
            numPages_ = 0;
            slots_ = 0;
            numFreeSlots_ = 0;
        }

        // Assigns a size class to this span, and it can be used for allocations
        void Commit(uint32_t slotSize) {
            DASSERT(IsDecommitted());
            DASSERT(slotSize);
            DASSERT(slotSize >= internal::kMinSlotSize &&
                    slotSize <= internal::kMaxSlotSize);
            const auto numSlots =
                uint16_t((numPages_ * internal::kPageSize) / slotSize);
            DASSERT(numSlots <= internal::kMaxSpanSize);
            slotSize_ = slotSize;
            numFreeSlots_ = GetNumSlots();
            GetHeap()->numFreePages_ -= numPages_;

            if (AllocationTracker* tracker = GetHeap()->tracker_) {
                tracker->DidCommitSpan(
                    AllocationTracker::SpanID{
                        .heapAddress = (uintptr_t)GetHeap(),
                        .pageIndex = GetPageIndex()},
                    AllocationTracker::SpanInfo{
                        .sizeClass = slotSize_ >> internal::kMinSlotSizeShift,
                        .numPages = (uint32_t)numPages_,
                        .numSlots = GetNumSlots(),
                        .numFreeSlots = numFreeSlots_});
            }
        }

        // Decommitted span can be reused with a different size class
        void Decommit() {
            DASSERT(IsCommitted());
            DASSERT(IsEmpty());
            if (AllocationTracker* tracker = GetHeap()->tracker_) {
                tracker->DidDecommitSpan(AllocationTracker::SpanID{
                    .heapAddress = (uintptr_t)GetHeap(),
                    .pageIndex = GetPageIndex()});
            }
            slotSize_ = 0;
            GetHeap()->numFreePages_ += numPages_;
        }

    public:
        // Return offset in bytes into the backed heap
        uint32_t AllocateSlot() {
            DASSERT(IsCommitted());
            DASSERT(IsActive() || IsEmpty());
            uint32_t slot = 0;
            for (auto i = 0; i < GetNumSlots(); ++i) {
                const bool isFree = !slots_.test(i);
                if (isFree) {
                    slots_.set(i);
                    slot = i;
                    break;
                }
            }
            --numFreeSlots_;
            const uint32_t pageBaseOffset = GetPageIndex() * internal::kPageSize;
            uint32_t offset = pageBaseOffset + (slot * slotSize_);

            if (AllocationTracker* tracker = GetHeap()->tracker_) {
                tracker->DidAllocateSlot(
                    AllocationTracker::SpanID{
                        .heapAddress = (uintptr_t)GetHeap(),
                        .pageIndex = GetPageIndex()},
                    AllocationTracker::SlotAllocInfo{.slotIndex = slot});
            }
            return offset;
        }

        void FreeSlot(uint32_t slotOffset) {
            DASSERT(IsCommitted());
            DASSERT(IsActive() || IsFull());
            const uint32_t pageBaseOffset = GetPageIndex() * internal::kPageSize;
            // Check for valid offset
            DASSERT(((slotOffset - pageBaseOffset) % slotSize_) == 0);
            DASSERT(slotOffset < (numPages_ * internal::kPageSize + pageBaseOffset));
            const uint32_t slot = (slotOffset - pageBaseOffset) / slotSize_;
            // Check for double free
            DASSERT(slots_.test(slot));
            slots_.reset(slot);
            ++numFreeSlots_;

            if (AllocationTracker* tracker = GetHeap()->tracker_) {
                tracker->DidFreeSlot(
                    AllocationTracker::SpanID{
                        .heapAddress = (uintptr_t)GetHeap(),
                        .pageIndex = GetPageIndex()},
                    AllocationTracker::SlotAllocInfo{.slotIndex = slot});
            }
        }

    public:
        // Split this span into two with 'numPages' in this
        Span* Split(uint32_t numPages) {
            DASSERT(IsDecommitted());
            DASSERT(numPages);
            DASSERT((uint32_t)numPages_ > numPages);
            const uint32_t numPagesInRight = numPages_ - numPages;
            Span* right = PageIndexToSpan(GetPageIndex() + numPages);
            // Update the spans
            Init(numPages);
            right->Init(numPagesInRight);
            return right;
        }

        // Returns new merged span
        Span* MergeRight(Span* right) {
            DASSERT(IsDecommitted());
            DASSERT(right);
            DASSERT(right->GetHeap() == GetHeap());
            DASSERT(right->IsDecommitted());
            const uint32_t rightNumPages = right->GetNumPages();
            right->Clear();
            Init(numPages_ + rightNumPages);
            return this;
        }

        // Returns new merged span
        Span* MergeLeft(Span* left) {
            DASSERT(IsDecommitted());
            DASSERT(left);
            DASSERT(left->GetHeap() == GetHeap());
            DASSERT(left->IsDecommitted());
            const uint32_t rightNumPages = GetNumPages();
            Clear();
            left->Init(left->GetNumPages() + rightNumPages);
            return left;
        }

        // Should be called on the first page
        Span* GetRightNeighbour() const {
            DASSERT(IsInFirstPage());
            const uint32_t pageIndex = GetPageIndex();
            const uint32_t nextSpanPageIndex = pageIndex + numPages_;
            if (nextSpanPageIndex >= GetHeap()->numPages_) {
                return nullptr;
            }
            return PageIndexToSpan(nextSpanPageIndex);
        }

        // Should be called on the first page
        Span* GetLeftNeighbour() const {
            DASSERT(IsInFirstPage());
            const uint32_t pageIndex = GetPageIndex();
            if (pageIndex == 0) {
                return nullptr;
            }
            Span* boundaryPageSpan = PageIndexToSpan(pageIndex - 1);
            DASSERT(boundaryPageSpan->IsInFirstPage() ||
                    boundaryPageSpan->IsInLastPage());
            if (boundaryPageSpan->IsInLastPage()) {
                return PageIndexToSpan(pageIndex -
                                       (-boundaryPageSpan->numPages_));
            } else {
                return boundaryPageSpan;
            }
        }

    public:
        constexpr bool IsEmpty() const {
            return numFreeSlots_ == GetNumSlots() && !IsDecommitted();
        }
        constexpr bool IsFull() const { return numFreeSlots_ == 0; }
        // Can be of too large size so need to be split
        // When newly created
        constexpr bool IsDecommitted() const { return slotSize_ == 0; }
        constexpr bool IsCommitted() const { return !IsDecommitted(); }

        constexpr bool IsActive() const {
            return numFreeSlots_ && numFreeSlots_ < GetNumSlots();
        }

        // Check whethe this span metadata is located in a boundary page of a
        // span
        constexpr bool IsInBoundaryPage() const { return numPages_ != 0; }
        constexpr bool IsInFirstPage() const { return numPages_ > 0; }
        constexpr bool IsInLastPage() const { return numPages_ < 0; }

        constexpr uint32_t GetNumPages() const { return numPages_; }

        HeapMetadata* GetHeap() const { return heap_; }

        uint32_t GetSizeClass() const { 
            return slotSize_ / internal::kMinSlotSize; 
        }

    private:
        Span* PageIndexToSpan(size_t index) const {
            return &GetHeap()->pageMetadata_[index];
        }

        // Returns an index of the page metadata this span resides in
        uint32_t GetPageIndex() const {
            return (uint32_t)(this - GetHeap()->pageMetadata_.data());
        }

        constexpr void InitAsLast(uint32_t numPages) {
            Clear();
            numPages_ = -int32_t(numPages);
        }

        constexpr uint16_t GetNumSlots() const {
            return (uint16_t)((numPages_ * internal::kPageSize) / slotSize_);
        }

    private:
        HeapMetadata* heap_;
        // Positive for the first page of a span
        // Negative for the last page of a span
        // Example span of 4 pages: [4][][][-4]
        int32_t numPages_;
        uint32_t slotSize_;
        // Bit is 1 for used
        std::bitset<internal::kMaxNumSlotsPerSpan> slots_;
        uint16_t numFreeSlots_;
    };

public:
    HeapMetadata(RefCountedPtr<ID3D12Heap> d3d12Heap,
                 std::string name,
                 AllocationTracker* tracker = nullptr)
        : name_(name)
        , heap_(std::move(d3d12Heap))
        , numPages_(uint32_t(heap_->GetDesc().SizeInBytes / internal::kPageSize))
        , numFreePages_(numPages_)
        , tracker_(tracker) {
        if (tracker_) {
            tracker_->DidCreateHeap(AllocationTracker::HeapInfo{
                .name = name, 
                .address = (uintptr_t)this,
                .size = (uint32_t)heap_->GetDesc().SizeInBytes});
        }
        pageMetadata_.resize(numPages_, Span(this));
    }

    ~HeapMetadata() {
        // DASSERT proper cleanup
        DASSERT(numFreePages_ == numPages_);
        Span& span = pageMetadata_[0];
        span.Clear();
        if (tracker_) {
            tracker_->DidDestroyHeap((uintptr_t)this);
        }
    }

    // Returns a span of the entire heap
    // Should be called only for an empty heap
    Span* GetRootSpan() {
        DASSERT(IsEmpty());
        Span& span = pageMetadata_[0];
        span.Init(numPages_);
        return &span;
    }

    constexpr Span* GetSpanFromByteOffset(uint32_t byteOffset) {
        uint32_t pageIndex = byteOffset / internal::kPageSize;
        DASSERT(pageIndex < numPages_);
        pageIndex++;
        // The page could be located anywhere inside the span
        // so find the first page of the span
        while (pageIndex-- > 0) {
            Span* span = &pageMetadata_[pageIndex];
            if (!span->IsDecommitted()) {
                return span;
            }
        }
        return nullptr;
    }

    constexpr ID3D12Heap* GetHeap() { return heap_.Get(); }

    constexpr bool IsEmpty() const { return numFreePages_ == numPages_; }

private:
    const std::string name_;
    const RefCountedPtr<ID3D12Heap> heap_;
    const uint32_t numPages_;
    AllocationTracker* const tracker_;

    uint32_t numFreePages_;
    // Only the first and the last page of a span contains data
    // Heap allocated array
    std::vector<Span> pageMetadata_;
};

using Span = HeapMetadata::Span;

// A logical grouping of spans of the same size class
// All spans are listed here
// Does not own spans
class Bucket {
public:
    constexpr void Init(uint32_t sizeClass) { sizeClass_ = sizeClass; }

    constexpr uint32_t GetSpanSizeInPages() {
        const uint32_t objectSize = sizeClass_ * internal::kMinSlotSize;
        const bool isSinglePageSpan =
            (internal::kPageSize / objectSize) >= internal::kMinSpanSize;
        if (isSinglePageSpan) {
            return 1;
        }
        const uint32_t spanSizeBytes = objectSize * internal::kMinSpanSize;
        return spanSizeBytes / internal::kPageSize;
    }

    // Lazily updates lists: full and active
    constexpr Span* GetActiveSpan() {
        Span* activeSpan = activeSpanHead_;
        while (activeSpan) {
            Span* next = activeSpan->next;
            if (activeSpan->IsFull()) {
                DListRemove(activeSpanHead_, activeSpan);
                DListPush(fullSpanHead_, activeSpan);
            } else {
                return activeSpan;
            }
            activeSpan = next;
        }
        // Check full spans
        for (Span* fullSpan : ListIterate(fullSpanHead_)) {
            if (!fullSpan->IsFull()) {
                DListRemove(fullSpanHead_, fullSpan);
                DListPush(activeSpanHead_, fullSpan);
                return fullSpan;
            }
        }
        return nullptr;
    }

    constexpr void RemoveSpan(Span* span) {
        DASSERT(span->IsActive() || span->IsEmpty());
        // An empty span can be in the full list
        // because we update our state in the GetActiveSpan()
        // E.g. the user only frees and not allocates
        if (ListContains(activeSpanHead_, span)) {
            DListRemove(activeSpanHead_, span);
        } else if (ListContains(fullSpanHead_, span)) {
            DListRemove(fullSpanHead_, span);
        } else {
            UNREACHABLE();
        }
    }

    constexpr void EmplaceSpan(Span* span) {
        DASSERT(span->IsActive() || span->IsEmpty());
        DASSERT(!ListContains(activeSpanHead_, span));
        DASSERT(!span->next && !span->prev);
        DListPush(activeSpanHead_, span);
    }

public:
    uint32_t sizeClass_;
    Span* activeSpanHead_;
    Span* fullSpanHead_;
};

// Contains a collection of all decommitted spans from all heaps
// Grouped by span pages count
class FreeSpanCache {
public:
    constexpr FreeSpanCache(uint32_t maxNumPages)
        : maxNumPages_(maxNumPages) {
        // + 1 for direct indexing by page size
        spanListHead_.resize(maxNumPages_ + 1);
    }

    constexpr void Push(Span* span) {
        DASSERT(span->IsDecommitted());
        DASSERT(!span->next && !span->prev);
        Span*& head = spanListHead_[span->GetNumPages()];
        DASSERT(!ListContains(head, span));
        DListPush(head, span);
    }

    constexpr Span* PopFirstFit(uint32_t numPages) {
        DASSERT(numPages <= maxNumPages_);
        Span* foundSpan = nullptr;
        for (uint32_t i = numPages; i <= maxNumPages_; ++i) {
            Span*& span = spanListHead_[i];
            if (!span) {
                continue;
            }
            foundSpan = span;
            DListPop(span);
            break;
        }
        return foundSpan;
    }

    constexpr void Remove(Span* span) {
        DASSERT(span->IsDecommitted());
        Span*& head = spanListHead_[span->GetNumPages()];
        DASSERT(ListContains(head, span));
        DListRemove(head, span);
    }

private:
    // Contains free spans at the index of used pages
    // E.g. a span of 4 pages at index 4
    // Default 128 buckets
    // We use +1 for simple indexing
    std::vector<Span*> spanListHead_;
    const uint32_t maxNumPages_;
};


} // namespace internal
} // namespace d3d12