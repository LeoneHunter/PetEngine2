#pragma once
#include "base/common.h"

// Owning ref counter
class RefCountedBase {
public:
    explicit RefCountedBase(uint64_t count = 1) : refCount_(count) {}

    void AddRef() { refCount_.fetch_add(1, std::memory_order_relaxed); }

    void Release() {
        DASSERT(refCount_ > 0);
        if (refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    uint64_t GetRefCount() const {
        return refCount_.load(std::memory_order_acq_rel);
    }

private:
    std::atomic<uint64_t> refCount_;
};


// Calls AddRef() and Release()
// Similar to boost::intrusive_ptr
template <class T>
class RefCountedPtr {
public:
    template <class U>
    friend class RefCountedPtr;

    constexpr RefCountedPtr() : ptr(nullptr) {}

    constexpr RefCountedPtr(nullptr_t) {}

    explicit RefCountedPtr(T* ptr) {
        this->ptr = ptr;
        if (ptr) {
            ptr->AddRef();
        }
    }

    RefCountedPtr(const RefCountedPtr& rhs) {
        ptr = rhs.ptr;
        if (ptr) {
            ptr->AddRef();
        }
    }

    template <class Other>
        requires std::convertible_to<Other*, T*>
    RefCountedPtr(const RefCountedPtr<Other>& rhs) {
        ptr = rhs.ptr;
        if (ptr) {
            ptr->AddRef();
        }
    }

    RefCountedPtr(RefCountedPtr&& rhs) {
        ptr = rhs.ptr;
        rhs.ptr = nullptr;
    }

    template <typename Other>
        requires std::convertible_to<Other*, T*>
    explicit RefCountedPtr(RefCountedPtr<Other>&& rhs) {
        ptr = rhs.ptr;
        rhs.ptr = nullptr;
    }

    ~RefCountedPtr() {
        if (ptr) {
            ptr->Release();
        }
    }

    RefCountedPtr& operator=(T* rhs) {
        if (ptr != rhs) {
            T* old = ptr;
            ptr = rhs;
            if (ptr) {
                ptr->AddRef();
            }
            if (old) {
                old->Release();
            }
        }
        return *this;
    }

    RefCountedPtr& operator=(const RefCountedPtr& rhs) {
        RefCountedPtr(rhs).Swap(*this);
        return *this;
    }

    template <typename Other>
        requires std::convertible_to<Other*, T*>
    RefCountedPtr& operator=(const RefCountedPtr<Other>& rhs) {
        RefCountedPtr(rhs).Swap(*this);
        return *this;
    }

    RefCountedPtr& operator=(RefCountedPtr&& rhs) {
        RefCountedPtr(std::move(rhs)).Swap(*this);
        return *this;
    }

    template <typename Other>
        requires std::convertible_to<Other*, T*>
    RefCountedPtr& operator=(RefCountedPtr<Other>&& rhs) {
        RefCountedPtr(std::move(rhs)).Swap(*this);
        return *this;
    }

    void Swap(RefCountedPtr& rhs) { std::swap(ptr, rhs.ptr); }

    constexpr const T* operator->() const { return ptr; }
    constexpr const T& operator*() const { return ptr; }

    constexpr T* operator->() { return ptr; }
    constexpr T& operator*() { return ptr; }

    constexpr T* Get() const { return ptr; }

    constexpr operator bool() const { return !!ptr; }

    // For windows IID_PPV
    constexpr T** operator&() { return &ptr; }

private:
    T* ptr;
};

template <class T1, class T2>
constexpr bool operator==(const RefCountedPtr<T1>& left,
                          const RefCountedPtr<T2>& right) {
    return left.Get() == right.Get();
}

template <class T1, class T2>
constexpr std::strong_ordering operator<=>(const RefCountedPtr<T1>& left,
                                           const RefCountedPtr<T2>& right) {
    return left.Get() <=> right.Get();
}

template <class T, class... Arg>
    requires std::constructible_from<T, Arg...>
RefCountedPtr<T> MakeRefCounted(Arg&&... arg) {
    return RefCountedPtr<T>(new T(std::forward<Arg>(arg)...));
}

template <class T>
RefCountedPtr<T> GetRef(T* object) {
    return RefCountedPtr<T>(object);
}



// Ref-counter for the WeakPtr
// Stored separatly from the object on heap
// Owned both by the object and references, so that references can detect that
//   the object has been deleted
class WeakRefCountedBase {
public:
    constexpr WeakRefCountedBase() = default;

    WeakRefCountedBase(const WeakRefCountedBase&) = delete;
    WeakRefCountedBase& operator=(const WeakRefCountedBase&) = delete;

    void IncWref() { ++Weaks; }

    void DecWref() {
        --Weaks;
        if (Weaks == 0 && bDestructed) {
            delete this;
        }
    }

    void OnDestructed() {
        bDestructed = true;
        if (Weaks == 0) {
            delete this;
        }
    }

    bool IsAlive() const { return !bDestructed; }

    uint64_t Weaks = 0;
    bool bDestructed = false;
};


/*
 * A RAII object that has a shared RefCounter to a controlled object
 * When object is deleted a RefCounter is notified
 * Doesn't own the controlled object
 */
template <typename T>
class WeakPtr {
public:
    constexpr WeakPtr() {}

    constexpr WeakPtr(nullptr_t) {}

    WeakPtr(T* inPtr, WeakRefCountedBase* inRC) {
        Ptr = inPtr;
        RC = inRC;
        RC->IncWref();
    }

    WeakPtr(const WeakPtr& inRight) {
        if (inRight.RC && inRight.RC->IsAlive()) {
            Ptr = inRight.Ptr;
            RC = inRight.RC;
            RC->IncWref();
        }
    }

    WeakPtr(WeakPtr&& inRight) {
        if (inRight.RC && inRight.RC->IsAlive()) {
            Ptr = inRight.Ptr;
            RC = inRight.RC;
        }
        inRight.Ptr = nullptr;
        inRight.RC = nullptr;
    }

    ~WeakPtr() {
        if (RC) {
            RC->DecWref();
        }
    }

    WeakPtr& operator=(const WeakPtr& inRight) {
        if (inRight.RC && inRight.RC->IsAlive()) {
            Reset();
            Ptr = inRight.Ptr;
            RC = inRight.RC;
            RC->IncWref();
        }
        return *this;
    }

    WeakPtr& operator=(WeakPtr&& inRight) noexcept {
        Reset();
        if (inRight.RC && inRight.RC->IsAlive()) {
            Ptr = inRight.Ptr;
            RC = inRight.RC;
        }
        inRight.Ptr = nullptr;
        inRight.RC = nullptr;
        return *this;
    }

    WeakPtr& operator=(nullptr_t) noexcept {
        Reset();
        return *this;
    }

    void Reset() {
        if (RC) {
            RC->DecWref();
        }
        RC = nullptr;
        Ptr = nullptr;
    }

    explicit operator bool() const { return RC && RC->IsAlive(); }

    T* operator->() { return Ptr; }
    const T* operator->() const { return Ptr; }

    T* operator*() { return Ptr; }
    const T* operator*() const { return Ptr; }

    T* Get() { return Ptr; }
    const T* Get() const { return Ptr; }

    T* GetChecked() { return RC ? RC->IsAlive() ? Ptr : nullptr : nullptr; }
    const T* GetChecked() const {
        return RC ? RC->IsAlive() ? Ptr : nullptr : nullptr;
    }

    bool Expired() const { return !RC || !RC->IsAlive(); }

private:
    WeakRefCountedBase* RC = nullptr;
    T* Ptr = nullptr;
};

template <class T1, class T2>
constexpr bool operator==(const WeakPtr<T1>& lhs, const WeakPtr<T2>& rhs) {
    return (uintptr_t)lhs.Get() == (uintptr_t)rhs.Get();
}

template <class T>
constexpr bool operator==(const WeakPtr<T>& lhs, const T* inPtr) {
    return lhs.Get() == inPtr;
}