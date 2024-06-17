#pragma once
#include "runtime/util.h"
#include "device_dx12.h"

#include <bitset>
#include <variant>
#include <expected>

namespace dx12 {

// Passed to Allocator to track all allocations
// Could be used for validation, debug or statistics
class AllocationTracker {
public:

    struct SpanID {
        uintptr_t heapAddress;
        uint32_t  pageIndex;

        auto operator<=>(const SpanID&) const = default;
    };

    struct SpanInfo {
        uint32_t  sizeClass;
        uint32_t  numPages;
        uint16_t  numSlots;
        uint16_t  numFreeSlots;
    };

    struct SlotAllocInfo {
        uint32_t slotIndex;
    };

    struct HeapInfo {
        HeapResourceType type;
        std::string      name;
        uintptr_t        address;
    };

    virtual ~AllocationTracker() = default;

    virtual void DidCreateHeap(const HeapInfo& info) {}
    virtual void DidDestroyHeap(uintptr_t address) {}

    virtual void DidCommitSpan(const SpanID& id, const SpanInfo& info) {}
    virtual void DidDecommitSpan(const SpanID& id) {}

    virtual void DidAllocateSlot(const SpanID& id, const SlotAllocInfo& info) {}
    virtual void DidFreeSlot(const SpanID& id, const SlotAllocInfo& info) {}
};



namespace internal {

// 64 KiB
// D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
constexpr size_t kMinAlignmentShift = 16;
constexpr size_t kMinAlignment = 1 << kMinAlignmentShift;

// 4 MiB
// D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
constexpr size_t kMaxAlignmentShift = 22;
constexpr size_t kMaxAlignment = 1 << kMaxAlignmentShift;

// 64 KiB == kMinAlignment
constexpr size_t kMinSlotSizeShift = kMinAlignmentShift;
constexpr size_t kMinSlotSize = 1 << kMinSlotSizeShift;
// 4 MiB
constexpr size_t kMaxSlotSizeShift = kMaxAlignmentShift;
constexpr size_t kMaxSlotSize = 1 << kMaxSlotSizeShift;

// 64 buckets at the 64KiB granularity
// Ignore 0 bucket. From 1 to 64
constexpr size_t kNumBuckets = kMaxSlotSize / kMinSlotSize;

// 2 MiB
// One page can contain 32 objects of the smallest size class
// And one object of the largest class requires 2 pages
constexpr size_t kPageSize = kMinSlotSize * 32;

constexpr size_t kNumPagesPerHeap = 128;
// 256 MiB
constexpr size_t kHeapSize = kPageSize * kNumPagesPerHeap;
constexpr size_t kHeapAlignment = kMaxAlignment;

// 8 objects in a span for large resources
constexpr size_t kMinSpanSize = 8;
constexpr size_t kMaxSpanSize = kPageSize / kMinSlotSize;

// Maximum pages per span
// For a span of the largest bucket objects
// 16
constexpr size_t kMaxNumPagesPerSpan = (kMinSpanSize * kMaxSlotSize) / kPageSize;

constexpr size_t kGiB = 1024 * 1024 * 1024;
constexpr size_t kMaxNumSpansPerGiB = (kGiB / kHeapSize) * kNumPagesPerHeap;

// 8 KiB
constexpr size_t kHeapMetadataAlignment = 1 << 13;

// 1GiB of space
constexpr size_t kMaxNumRetiredHeaps = 4;

constexpr uint32_t SizeClassToSlotSize(uint32_t sizeClass) {
    return sizeClass * kMinSlotSize;
}

} // namespace internal

namespace internal {

// Manages a ID3D12Heap carved up into kNumPagesPerHeap pages
// Contains the metadata about allocations done on the gpu side
// When object is requested these pages are connected into spans
//     and a slot is allocated from such span
class alignas(kHeapMetadataAlignment) HeapMetadata {
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
    class Span {
    public:

        constexpr Span() = default;
        
        constexpr ~Span() {
            DASSERT(!next && !prev);
        }

        // Initialize as brand new erasing old state completely
        void Init(uint32_t numPages, uint32_t slotSize = 0) {
            DASSERT(IsDecommitted());
            DASSERT(slots_ == 0);
            DASSERT(numPages);
            // Clean previous state
            if(numPages_) {
                const uint32_t lastPageIndex = GetPageIndex() + numPages - 1;
                PageIndexToSpan(lastPageIndex)->Clear();
                Clear();
            }
            numPages_ = numPages;
            if(slotSize) {
                Commit(slotSize);
            }
            if(numPages > 1) {
                const uint32_t lastPageIndex = GetPageIndex() + numPages - 1;
                DASSERT(lastPageIndex < kNumPagesPerHeap);
                Span* lastPageMetadata = PageIndexToSpan(lastPageIndex);
                lastPageMetadata->InitAsLast(numPages);
            }
        }

        constexpr void Clear() {
            slotSize_ = 0;
            numPages_ = 0;
            slots_ = 0;
            numSlots_ = 0;
            numFreeSlots_ = 0;
        }

        // Assigns a size class to this span, and it can be used for allocations
        void Commit(uint32_t slotSize) {
            DASSERT(IsDecommitted());
            DASSERT(slotSize);
            DASSERT(slotSize >= kMinSlotSize && slotSize <= kMaxSlotSize);
            const uint16_t numSlots = uint16_t((numPages_ * kPageSize) / slotSize);
            DASSERT(numSlots <= kMaxSpanSize);
            slotSize_ = slotSize;
            numSlots_ = numSlots;
            numFreeSlots_ = numSlots_;
            GetHeap()->numFreePages_ -= numPages_;

            if(AllocationTracker* tracker = GetHeap()->tracker_) {
                tracker->DidCommitSpan(
                    AllocationTracker::SpanID{
                        .heapAddress = (uintptr_t)GetHeap(),
                        .pageIndex = GetPageIndex()},
                    AllocationTracker::SpanInfo{
                        .sizeClass = slotSize_ >> kMinSlotSizeShift,
                        .numPages = (uint32_t)numPages_,
                        .numSlots = numSlots_,
                        .numFreeSlots = numFreeSlots_}
                );
            }
        }

        // Decommitted span can be reused with a different size class
        void Decommit() {
            DASSERT(IsCommitted());
            DASSERT(IsEmpty());
            if(AllocationTracker* tracker = GetHeap()->tracker_) {
                tracker->DidDecommitSpan(
                    AllocationTracker::SpanID{
                        .heapAddress = (uintptr_t)GetHeap(),
                        .pageIndex = GetPageIndex()}
                );
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
            for(auto i = 0; i < numSlots_; ++i) {
                const bool isFree = !slots_.test(i);
                if(isFree) {
                    slots_.set(i);
                    slot = i;
                    break;
                }
            }
            --numFreeSlots_;
            const uint32_t pageBaseOffset = GetPageIndex() * kPageSize;
            uint32_t offset = pageBaseOffset + (slot * slotSize_);

            if(AllocationTracker* tracker = GetHeap()->tracker_) {
                tracker->DidAllocateSlot(
                    AllocationTracker::SpanID{
                        .heapAddress = (uintptr_t)GetHeap(),
                        .pageIndex = GetPageIndex()},
                    AllocationTracker::SlotAllocInfo{
                        .slotIndex = slot}
                );
            }
            return offset;
        }

        void FreeSlot(uint32_t slotOffset) {
            DASSERT(IsCommitted());
            DASSERT(IsActive() || IsFull());
            const uint32_t pageBaseOffset = GetPageIndex() * kPageSize;
            // Check for valid offset
            DASSERT(((slotOffset - pageBaseOffset) % slotSize_) == 0);
            DASSERT(slotOffset < (numPages_ * kPageSize + pageBaseOffset));
            const uint32_t slot = (slotOffset - pageBaseOffset) / slotSize_;
            // Check for double free
            DASSERT(slots_.test(slot));
            slots_.reset(slot);
            ++numFreeSlots_;

            if(AllocationTracker* tracker = GetHeap()->tracker_) {
                tracker->DidFreeSlot(
                    AllocationTracker::SpanID{
                        .heapAddress = (uintptr_t)GetHeap(),
                        .pageIndex = GetPageIndex()},
                    AllocationTracker::SlotAllocInfo{
                        .slotIndex = slot}
                );
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
            const size_t pageIndex = GetPageIndex();
            const size_t nextSpanPageIndex = pageIndex + numPages_;
            if(nextSpanPageIndex >= kNumPagesPerHeap) {
                return nullptr;
            }
            return PageIndexToSpan(nextSpanPageIndex);
        }
        
        // Should be called on the first page
        Span* GetLeftNeighbour() const {
            DASSERT(IsInFirstPage());
            const size_t pageIndex = GetPageIndex();
            if(pageIndex == 0) {
                return nullptr;
            }
            Span* boundaryPageSpan = PageIndexToSpan(pageIndex - 1);
            DASSERT(boundaryPageSpan->IsInFirstPage() || 
                    boundaryPageSpan->IsInLastPage());
            if(boundaryPageSpan->IsInLastPage()) {
                return PageIndexToSpan(pageIndex - (-boundaryPageSpan->numPages_));
            } else {
                return boundaryPageSpan;
            }
        }
            
    public:
        constexpr bool IsEmpty() const { 
            return numFreeSlots_ == numSlots_ && !IsDecommitted();
        }
        constexpr bool IsFull() const { return numFreeSlots_ == 0; }
        // Can be of too large size so need to be split
        // When newly created
        constexpr bool IsDecommitted() const { return slotSize_ == 0; }
        constexpr bool IsCommitted() const { return !IsDecommitted(); }

        constexpr bool IsActive() const { 
            return numFreeSlots_ && numFreeSlots_ < numSlots_;
        }

        // Check whethe this span metadata is located in a boundary page of a span
        constexpr bool IsInBoundaryPage() const { return numPages_ != 0; }
        constexpr bool IsInFirstPage() const { return numPages_ > 0; }
        constexpr bool IsInLastPage() const { return numPages_ < 0; }

        constexpr uint32_t GetNumPages() const { return numPages_; }

        HeapMetadata* GetHeap() const { 
            return reinterpret_cast<HeapMetadata*>(
                    AlignDown((uintptr_t)this, kHeapMetadataAlignment));
        }

        uint32_t GetSizeClass() const { return slotSize_ / kMinSlotSize; }

    private:
        Span* PageIndexToSpan(size_t index) const { 
            return &GetHeap()->pageMetadata_[index];
        } 

        // Returns an index of the page metadata this span resides in
        uint32_t GetPageIndex() const {
            return (uint32_t)(this - GetHeap()->pageMetadata_);
        }
        
        constexpr void InitAsLast(uint32_t numPages) {
            Clear();
            numPages_ = -int64_t(numPages);
        }

    private:
        // Positive for the first page of a span
        // Negative for the last page of a span
        // Example span of 4 pages: [4][][][-4]
        int32_t                   numPages_;
        uint32_t                  slotSize_;
        // Bit is 1 for used
        std::bitset<kMaxSpanSize> slots_;
        uint16_t                  numSlots_;
        uint16_t                  numFreeSlots_;

    public:
        Span* next = nullptr;
        Span* prev = nullptr;
    };

public:
    HeapMetadata(HeapResourceType type, 
                 RefCountedPtr<ID3D12Heap> d3d12Heap,
                 std::string name,
                 AllocationTracker* tracker = nullptr) 
        : name_(name)
        , type_(type)
        , desc_(d3d12Heap->GetDesc())
        , heap_(std::move(d3d12Heap))
        , numFreePages_(kNumPagesPerHeap)
        , tracker_(tracker) {
        if(tracker_) {
            tracker_->DidCreateHeap(AllocationTracker::HeapInfo{
                .type = type,
                .name = name,
                .address = (uintptr_t)this
            });
        }
    }

    ~HeapMetadata() {
        // DASSERT proper cleanup
        DASSERT(numFreePages_ == kNumPagesPerHeap); 
        Span& span = pageMetadata_[0];
        span.Clear();
        if(tracker_) {
            tracker_->DidDestroyHeap((uintptr_t)this);
        }
    }

    // Creates a span of entire heap
    // Should be called only during initialization
    Span* CreateInitSpan() {
        Span& span = pageMetadata_[0];
        span.Init(kNumPagesPerHeap);
        return &span;
    }

    constexpr Span* GetSpanFromByteOffset(size_t byteOffset) {
        size_t pageIndex = AlignDown(byteOffset, kPageSize) / kPageSize;
        DASSERT(pageIndex < kNumPagesPerHeap);
        pageIndex++;
        // The page could be located anywhere inside the span
        // so find the first page of the span
        while(pageIndex --> 0) {
            Span* span = &pageMetadata_[pageIndex];
            if(!span->IsDecommitted()) {
                return span;
            }
        }
        return nullptr;
    }

    constexpr ID3D12Heap* GetHeap() { return heap_.Get(); }

    constexpr HeapResourceType GetType() const { return type_; }

    constexpr bool IsEmpty() const { return numFreePages_ == kNumPagesPerHeap; }

public:
    HeapMetadata* next = nullptr;
    HeapMetadata* prev = nullptr;

private:
    std::string                 name_;
    HeapResourceType            type_;
    D3D12_HEAP_DESC             desc_;
    RefCountedPtr<ID3D12Heap>   heap_;
    size_t                      numFreePages_;
    // Only the first and the last page of a span contains data
    Span                        pageMetadata_[kNumPagesPerHeap]{};
    AllocationTracker* const    tracker_;
};

static_assert(kHeapMetadataAlignment >= sizeof(HeapMetadata));

using Span = HeapMetadata::Span;

// A logical grouping of spans of the same size class
// All spans are listed here
// Does not own spans
class Bucket {
public:

    constexpr void Init(uint32_t sizeClass) {
        sizeClass_ = sizeClass;
    }

    constexpr uint32_t GetSpanSizeInPages() {
        const uint32_t objectSize = sizeClass_ * kMinSlotSize;
        const bool isSinglePageSpan = (kPageSize / objectSize) >= kMinSpanSize;
        if(isSinglePageSpan) {
            return 1;
        }
        const uint32_t spanSizeBytes = objectSize * kMinSpanSize;
        return spanSizeBytes / kPageSize;
    }

    // Lazily updates lists: full and active
    constexpr Span* GetActiveSpan() {
        Span* activeSpan = activeSpanHead_;
        while(activeSpan) {
            Span* next = activeSpan->next;
            if(activeSpan->IsFull()) {
                DListRemove(activeSpanHead_, activeSpan);
                DListPush(fullSpanHead_, activeSpan);
            } else {
                return activeSpan;
            }
            activeSpan = next;
        }
        // Check full spans
        for(Span* fullSpan: ListIterate(fullSpanHead_)) {
            if(!fullSpan->IsFull()) {
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
        if(ListContains(activeSpanHead_, span)) {
            DListRemove(activeSpanHead_, span);
        } else if(ListContains(fullSpanHead_, span)) {
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
    Span*    activeSpanHead_;
    Span*    fullSpanHead_;
};

// Contains a collection of all decommitted spans from all heaps
// Grouped by span pages count
class FreeSpanCache {
public:

    constexpr void Push(Span* span) {
        DASSERT(span->IsDecommitted());
        DASSERT(!span->next && !span->prev);
        Span*& head = spanListHead[span->GetNumPages()];
        DASSERT(!ListContains(head, span));
        DListPush(head, span);
    }

    constexpr Span* PopFirstFit(uint32_t numPages) {
        DASSERT(numPages <= kNumPagesPerHeap);
        Span* foundSpan = nullptr;
        for(uint32_t i = numPages; i <= kNumPagesPerHeap; ++i) {
            Span*& span = spanListHead[i];
            if(!span) {
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
        Span*& head = spanListHead[span->GetNumPages()];
        DASSERT(ListContains(head, span));
        DListRemove(head, span);
    }

private:
    // Contains free spans at the index of used pages
    // E.g. a span of 4 pages at index 4
    // Default 128 buckets
    // We use +1 for simple indexing
    Span* spanListHead[kNumPagesPerHeap + 1]{};
};

} // namespace internal

} // namespace dx12