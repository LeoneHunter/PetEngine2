#pragma once
#include "base/common.h"

// Simple pooled allocator
// Manages a collection of pages with each page containing kPageSize slots
// Object's destructors are not called so they should be trivial types
template<size_t kSlotSize, size_t kPageSize>
class PooledAllocator {
public:
    // Also used as a sentinel index if no more free slots
    static constexpr size_t kSlotsPerPage = kPageSize;
    // Align Page to the power of two
    // Should be larger than Page::slots array
    static constexpr size_t kPageAlignment = std::bit_ceil(kSlotSize * kSlotsPerPage);

public:

    constexpr PooledAllocator() {
        activePagesHead_ = new Page();
    }

    constexpr ~PooledAllocator() {
        ListDelete(activePagesHead_);
        ListDelete(fullPagesHead_);
        if(emptyPage_) {
            delete emptyPage_;
        }
    }

    PooledAllocator(const PooledAllocator&) = delete;
    PooledAllocator& operator=(const PooledAllocator&) = delete;

    PooledAllocator(PooledAllocator&&) = delete;
    PooledAllocator& operator=(PooledAllocator&&) = delete;

    constexpr void* Allocate() {
        Page* activePage = GetActive();
        if(!activePage) {
            if(!emptyPage_) {
                activePage = new Page();
            } else {
                activePage = emptyPage_;
                emptyPage_ = nullptr;
            }
            DListPush(activePagesHead_, activePage);
        }
        void* out = activePage->Allocate();
        if(activePage->Full()) {
            DListRemove(activePagesHead_, activePage);
            DListPush(fullPagesHead_, activePage);
        }
        return out;
    }

    constexpr void Free(void* object) {
        Page* page = Page::FromObject(object);
        const bool wasFull = page->Full();
        page->Free(object);

        if(wasFull) {
            DListRemove(fullPagesHead_, page);
            DListPush(activePagesHead_, page);

        } else if(page->Empty()) {
            if(emptyPage_) {
                delete emptyPage_;
            }
            emptyPage_ = page;
            DListRemove(activePagesHead_, page);
        }
    }

private:

    struct alignas(kPageAlignment) Page {

        union Slot {
            size_t next;
            char   mem[kSlotSize]{};
        };

        Slot        slots[kSlotsPerPage]{};
        size_t      nextFreeSlot = 0;
        size_t      freeSlotsNum = kSlotsPerPage;

        Page*       next = nullptr;
        Page*       prev = nullptr;

        // For xor validation
        uintptr_t   self = 0;

        static constexpr Page* FromObject(void* object) {
            const uintptr_t pagePtr = AlignDown((size_t)object, kPageAlignment);
            Page* page = reinterpret_cast<Page*>(pagePtr);
            DASSERT((pagePtr ^ page->self) == 0);
            return page;
        }

        constexpr Page() {
            for(size_t i = 0; i < kSlotsPerPage; ++i) {
                slots[i].next = EncodeNext(i + 1);
            }
            self = reinterpret_cast<uintptr_t>(this);
        }

        constexpr void Free(void* object) {
            const ptrdiff_t slotIndex = reinterpret_cast<Slot*>(object) - slots;
            DASSERT(slotIndex < kSlotsPerPage && slotIndex >= 0);
            Slot& slot = slots[slotIndex];
            DASSERT_F(DecodeNext(slot.next) > kSlotsPerPage,
                    "Next slot index encoded into the slot is inside the valid range. "
                    "Could be a double free");
            slot.next = EncodeNext(nextFreeSlot);
            nextFreeSlot = slotIndex;
            ++freeSlotsNum;
        }

        constexpr void* Allocate() {
            if(nextFreeSlot == kSlotsPerPage) {
                return nullptr;
            }
            Slot& slot = slots[nextFreeSlot];
            nextFreeSlot = DecodeNext(slot.next);
            DASSERT_F(nextFreeSlot <= kSlotsPerPage, 
                  "Next slot index out of range. Could be use-after-free");
            slot.next = 0;
            --freeSlotsNum;
            return &slot;
        }

        constexpr bool Full() const { 
            return nextFreeSlot == kSlotsPerPage;
        }

        constexpr bool Empty() const {
            return freeSlotsNum == kSlotsPerPage;
        }
        
    private:
        // For validation
        static constexpr uint32_t kFreeIndexOffset = 0xABCDEFEDU;

        constexpr size_t EncodeNext(size_t index) {
            return index + kFreeIndexOffset;
        }

        constexpr size_t DecodeNext(size_t index) {
            return index - kFreeIndexOffset;
        }
    };

    constexpr Page* GetActive() {
        return activePagesHead_;
    }

private:
    Page* activePagesHead_ = nullptr;
    Page* fullPagesHead_ = nullptr;
    Page* emptyPage_ = nullptr;
};