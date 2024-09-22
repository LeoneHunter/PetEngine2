#pragma once
#include "base/math_util.h"

// Bump allocator
// Only allocates memory
// Memory is freed at at once on destruction
class BumpAllocator {
public:
    constexpr static unsigned kDefaultPageSize = 64 * 1024;

    void* Allocate(size_t size, size_t alignment = sizeof(std::max_align_t)) {
        if(!head_) {
            AllocatePage(pageSize_);
            DASSERT(head_);
        }
        size_t allocSize = size;
        uintptr_t alignedPtr = AlignUp(ptr_, alignment);
        if(alignedPtr != ptr_) {
            allocSize += alignedPtr - ptr_;
        }
        // TODO: Handle dedicated allocation
        DASSERT(allocSize <= pageSize_);
        if(alignedPtr + allocSize > end_) {
            AllocatePage(pageSize_);
            alignedPtr = AlignUp(ptr_, alignment);
            DASSERT(head_);
        }
        ptr_ += allocSize;
        return (void*)alignedPtr;
    }

    // Only for POD types, destructor isn't called
    template<class T, class... Args>
        requires std::constructible_from<T, Args...>
    T* Allocate(Args&&... args) {
        static_assert(sizeof(T) > 0,
                      "value_type must be complete before calling allocate.");
        void* mem = Allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...); 
    }

    void* GetHeadForTesting() const { return head_; }

public:
    BumpAllocator(size_t pageSize = kDefaultPageSize)
        : pageSize_(std::bit_ceil(pageSize))
        , head_(nullptr)
        , ptr_(0)
        , end_(0) {}

    BumpAllocator(BumpAllocator&&) = default;

    BumpAllocator& operator=(BumpAllocator&& rhs) {
        BumpAllocator(std::move(rhs)).swap(*this);
        return *this;
    }

    void swap(BumpAllocator& rhs) {
        std::swap(head_, rhs.head_);
        std::swap(pageSize_, rhs.pageSize_);
        std::swap(ptr_, rhs.ptr_);
        std::swap(end_, rhs.end_);
    }

    ~BumpAllocator() {
        auto* page = head_;
        while (page) {
            auto* next = page->next;
            delete[] page;
            page = next;
        }
        head_ = nullptr;
    }

private:
    struct Page {
        Page* next;
    };

    void AllocatePage(size_t size) {
        auto* mem = new char[size + sizeof(Page)];
        auto* page = reinterpret_cast<Page*>(mem);
        page->next = head_;
        head_ = page;
        ptr_ = (uintptr_t)mem + sizeof(Page);
        end_ = ptr_ + size;
    }

private:
    size_t pageSize_;
    Page* head_;
    uintptr_t ptr_;
    uintptr_t end_;
};