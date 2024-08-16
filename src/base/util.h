#pragma once
#include "math_util.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <span>
#include <string>

constexpr Vec4 HashColorToVec4(std::string_view inHexCode) {
    DASSERT(!inHexCode.empty() && inHexCode.starts_with('#') &&
            (inHexCode.size() == 7 || inHexCode.size() == 9));
    Vec4 out;
    out.x = 0.f;
    out.y = 0.f;
    out.z = 0.f;
    out.w = 1.f;

    int temp{};
    int out1{};
    int out2{};
    const auto arraySize = inHexCode.size();

    for(size_t i = 1; i < arraySize; i++) {
        switch(inHexCode[i]) {
        case '0': temp = 0x0; break;
        case '1': temp = 0x1; break;
        case '2': temp = 0x2; break;
        case '3': temp = 0x3; break;
        case '4': temp = 0x4; break;
        case '5': temp = 0x5; break;
        case '6': temp = 0x6; break;
        case '7': temp = 0x7; break;
        case '8': temp = 0x8; break;
        case '9': temp = 0x9; break;
        case 'A':
        case 'a': temp = 0xA; break;
        case 'B':
        case 'b': temp = 0xB; break;
        case 'C':
        case 'c': temp = 0xC; break;
        case 'D':
        case 'd': temp = 0xD; break;
        case 'E':
        case 'e': temp = 0xE; break;
        case 'F':
        case 'f': temp = 0xF; break;
        default:  temp = 0x0; break;
        }
        if(i == 1 || i == 3 || i == 5 || i == 7) {
            out1 = temp;
        } else {
            out2 = temp;
        }
        if(i == 2) {
            out.x = static_cast<float>(out1 * 16 + out2) / 256.0f;
        } else if(i == 4) {
            out.y = static_cast<float>(out1 * 16 + out2) / 256.0f;
        } else if(i == 6) {
            out.z = static_cast<float>(out1 * 16 + out2) / 256.0f;
        } else if(i == 8) {
            out.w = static_cast<float>(out1 * 16 + out2) / 256.0f;
        }
    }
    return out;
}

/*
* Compile time checked hex color string representation
*/
struct HexCodeString {
public:

    template <class Ty> requires std::convertible_to<const Ty&, std::string_view>
    consteval HexCodeString(const Ty& Str_val)
        : Str(Str_val) {
        DASSERT(Str.size() == 7 || Str.size() == 9 && Str.starts_with('#') &&
            "Hex color wrong format. String should start with a # and contain 7 or 9 characters");
    }

    _NODISCARD constexpr std::string_view get() const {
        return Str;
    }

private:
    std::string_view Str;
};

// RGBA color 
// 0 - 1 normalized
class Color4f {
public:
    float r, g, b, a;

    constexpr static Color4f FromHex(std::string_view str) {
        DASSERT(str.starts_with('#'));
        DASSERT(str.size() == 7 || str.size() == 9);
        const int numComponents = ((int)str.size() - 1) / 2;
        const char* ptr = str.data() + 1;
        uint8_t out[4]{};
        for(int i = 0; i < numComponents; ++i) {
            std::from_chars_result result = std::from_chars(ptr, ptr + 2, out[i], 16);
            DASSERT(result.ec == std::errc());
            ptr += 2;
        }
        return {out[0] / 255.f, out[1] / 255.f, out[2] / 255.f, out[3] / 255.f};
    }

};

constexpr float Saturate(float f) { return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f; }
constexpr int F32toInt8(float f) { return ((int)(Saturate(f) * 255.0f + 0.5f)); }

/**
 * RGB color stored as a 32bit integer
 * Component ranges 0 to ff
 */
struct ColorU32 {
public:

    enum BitOffsets {
        R = 0,
        G = 8,
        B = 16,
        A = 24
    };

    constexpr static ColorU32 FromHex(std::string_view inStr) {
        auto vec = HashColorToVec4(inStr);
        return {vec.x, vec.y, vec.z, vec.w};
    }

    constexpr ColorU32(float R, float G, float B, float A): rgb() {
        rgb = ((uint32_t)F32toInt8(R)) << BitOffsets::R;
        rgb |= ((uint32_t)F32toInt8(G)) << BitOffsets::G;
        rgb |= ((uint32_t)F32toInt8(B)) << BitOffsets::B;
        rgb |= ((uint32_t)F32toInt8(A)) << BitOffsets::A;
    }

    constexpr ColorU32(): rgb(0xff) {}

    constexpr ColorU32& operator= (std::string_view inHexCode) {
        auto result = HashColorToVec4(inHexCode);
        SetComponent(BitOffsets::R, result.x);
        SetComponent(BitOffsets::G, result.y);
        SetComponent(BitOffsets::B, result.z);
        SetComponent(BitOffsets::A, result.w);
        return *this;
    }

    // Doesn't mutate the object
    // Helper to multiply alpa by a value
    constexpr ColorU32 MultiplyAlpha(float inValue) const {
        auto out = *this;
        out.SetComponent(BitOffsets::A, out.GetComponentAsFloat(BitOffsets::A) * Saturate(inValue));
        return out;
    }

    // Returns true is Alpa is 0
    constexpr bool IsTransparent() const {
        return GetComponentAsFloat(A) == 0.f;
    }

    constexpr float GetComponentAsFloat(BitOffsets inComponent) const {
        return (float)((rgb >> inComponent) & (uint8_t)0xff) / 255.f;
    }

    constexpr void SetComponent(BitOffsets inComponent, float inValue) {
        rgb &= ~((uint8_t)0xff << inComponent);
        rgb |= ((uint32_t)F32toInt8(inValue)) << inComponent;
    }

    constexpr operator uint32_t() const { return rgb; }

private:
    uint32_t rgb;
};

/**
 * RGB color with 4 float values
 */
struct ColorFloat4 {

    constexpr ColorFloat4()
        : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}

    constexpr ColorFloat4(ColorU32 inColor)
        : r(inColor.GetComponentAsFloat(ColorU32::R))
        , g(inColor.GetComponentAsFloat(ColorU32::G))
        , b(inColor.GetComponentAsFloat(ColorU32::B))
        , a(inColor.GetComponentAsFloat(ColorU32::A)) {}

    constexpr ColorFloat4(float inVal)
        : r(math::Clamp(inVal, 0.f, 1.f)), g(math::Clamp(inVal, 0.f, 1.f)), b(math::Clamp(inVal, 0.f, 1.f)), a(1.0f) {}

    constexpr ColorFloat4(float inR, float inG, float inB, float inA = 1.f)
        : r(math::Clamp(inR, 0.f, 1.f)), g(math::Clamp(inG, 0.f, 1.f)), b(math::Clamp(inB, 0.f, 1.f)), a(math::Clamp(inA, 0.f, 1.f)) {}

    constexpr ColorFloat4(const Vec4& inFloat4)
        : r(math::Clamp(inFloat4.x, 0.f, 1.f)), g(math::Clamp(inFloat4.y, 0.f, 1.f)), b(math::Clamp(inFloat4.z, 0.f, 1.f)), a(math::Clamp(inFloat4.w, 0.f, 1.f)) {}

    constexpr ColorFloat4(std::string_view inHexColor)
        : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {
        auto result = HashColorToVec4(inHexColor);
        r = result.x;
        g = result.y;
        b = result.z;
        a = result.w;
    }

    constexpr static ColorFloat4 FromHex(std::string_view inStr) {
        auto vec = HashColorToVec4(inStr);
        return {vec.x, vec.y, vec.z, vec.w};
    }

    constexpr explicit operator ColorU32() const { return {r, g, b, a}; }

    float r;
    float g;
    float b;
    float a;
};

namespace util {

    class RefCounter {
    public:

        constexpr RefCounter() = default;

        RefCounter(const RefCounter&) = delete;
        RefCounter& operator=(const RefCounter&) = delete;

        void IncWref() {
            ++Weaks;
        }

        void DecWref() {
            --Weaks;

            if(Weaks == 0 && bDestructed) {
                delete this;
            }
        }

        void OnDestructed() {
            bDestructed = true;

            if(Weaks == 0) {
                delete this;
            }
        }

        bool IsAlive() const {
            return !bDestructed;
        }

        uint64_t  Weaks = 0;
        bool bDestructed = false;
    };

    
    /*
    * A RAII object that has a shared RefCounter to a controlled object
    * When object is deleted a RefCounter is notified
    * Doesn't own the controlled object
    */
    template<typename T>
    class WeakPtr {
    public:

        constexpr WeakPtr() {}

        constexpr WeakPtr(nullptr_t) {}

        WeakPtr(T* inPtr, RefCounter* inRC) {
            Ptr = inPtr;
            RC = inRC;
            RC->IncWref();
        }

        WeakPtr(const WeakPtr& inRight) {
            if(inRight.RC && inRight.RC->IsAlive()) {
                Ptr = inRight.Ptr;
                RC = inRight.RC;
                RC->IncWref();
            }
        }

        WeakPtr(WeakPtr&& inRight) {
            if(inRight.RC && inRight.RC->IsAlive()) {
                Ptr = inRight.Ptr;
                RC = inRight.RC;
            }
            inRight.Ptr = nullptr;
            inRight.RC = nullptr;
        }

        ~WeakPtr() {
            if(RC) {
                RC->DecWref();
            }
        }

        WeakPtr& operator=(const WeakPtr& inRight) {
            if(inRight.RC && inRight.RC->IsAlive()) {
                Reset();
                Ptr = inRight.Ptr;
                RC = inRight.RC;
                RC->IncWref();
            }
            return *this;
        }

        WeakPtr& operator=(WeakPtr&& inRight) noexcept {
            Reset();
            if(inRight.RC && inRight.RC->IsAlive()) {
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
            if(RC) {
                RC->DecWref();
            }
            RC = nullptr;
            Ptr = nullptr;
        }

        explicit operator bool() const {
            return RC && RC->IsAlive();
        }

        T* operator->() { return Ptr; }
        const T* operator->() const { return Ptr; }

        T* operator*() { return Ptr; }
        const T* operator*() const { return Ptr; }

        T* Get() { return Ptr; }
        const T* Get() const { return Ptr; }

        T* GetChecked() { return RC ? RC->IsAlive() ? Ptr : nullptr : nullptr; }
        const T* GetChecked() const { return RC ? RC->IsAlive() ? Ptr : nullptr : nullptr; }

        bool Expired() const { return !RC || !RC->IsAlive(); }

    private:
        RefCounter* RC  = nullptr;
        T*			Ptr = nullptr;
    };

    template<class T1, class T2>
    constexpr bool operator==(const WeakPtr<T1>& lhs, const WeakPtr<T2>& rhs) {
        return (uintptr_t)lhs.Get() == (uintptr_t)rhs.Get();
    }

    template<class T>
    constexpr bool operator==(const WeakPtr<T>& lhs, const T* inPtr) {
        return lhs.Get() == inPtr;
    }
}

using util::WeakPtr;


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

private:
    std::atomic<uint64_t> refCount_;
};

// Calls AddRef() and Release()
// Similar to boost::intrusive_ptr
template<class T>
class RefCountedPtr {
public:

    template<class U>
    friend class RefCountedPtr;

    constexpr RefCountedPtr() : ptr(nullptr) {}

    constexpr RefCountedPtr(nullptr_t) {}

    explicit RefCountedPtr(T* ptr) {
        this->ptr = ptr;
        if(ptr) {
            ptr->AddRef();
        }
    }

    RefCountedPtr(const RefCountedPtr& rhs) {
        ptr = rhs.ptr;
        if(ptr) {
            ptr->AddRef();
        }
    }

    template<typename Other>
        requires std::convertible_to<Other*, T*>
    RefCountedPtr(const RefCountedPtr<Other>& rhs) {
        ptr = rhs.ptr;
        if(ptr) {
            ptr->AddRef();
        }
    }

    RefCountedPtr(RefCountedPtr&& rhs) {
        ptr = rhs.ptr;
        rhs.ptr = nullptr;
    }

    template<typename Other>
        requires std::convertible_to<Other*, T*>
    explicit RefCountedPtr(RefCountedPtr<Other>&& rhs) {
        ptr = rhs.ptr;
        rhs.ptr = nullptr;
    }

    ~RefCountedPtr() {
        if(ptr) {
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

    template<typename Other>
        requires std::convertible_to<Other*, T*>
    RefCountedPtr& operator=(const RefCountedPtr<Other>& rhs) {
        RefCountedPtr(rhs).Swap(*this);
        return *this;
    }

    RefCountedPtr& operator=(RefCountedPtr&& rhs) {
        RefCountedPtr(std::move(rhs)).Swap(*this);
        return *this;
    }

    template<typename Other>
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
    constexpr T** operator& () { return &ptr; }

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



template<class T>
struct ListNode {
    T* next = nullptr;
};

template<class T>
struct DListNode {
    T* next = nullptr;
    T* prev = nullptr;
};

template <class T>
concept IsDListNode = requires(T node) {
    { node.next } -> std::convertible_to<T*>;
    { node.prev } -> std::convertible_to<T*>;
};

template<class T>
concept IsListNode = !IsDListNode<T> && requires(T node) {
    { node.next } -> std::convertible_to<T*>;
};

template<class T>
    requires IsListNode<T>
constexpr void ListPush(T*& head, T* node) {
    node->next = head;
    head = node;
}

template<class T>
    requires IsListNode<T>
constexpr T* ListPop(T*& head) {
    if(!head) {
        return nullptr;
    }
    T* pop = head;
    head = head->next;
    pop->next = nullptr;
    return pop;
}

template<class T>
    requires IsListNode<T>
constexpr void ListDelete(T*& head) {
    while(T* node = ListPop(head)) {
        delete node;
    }
    head = nullptr;
}


template<class T>
    requires IsListNode<T> || IsDListNode<T>
constexpr bool ListContains(T* head, T* entry) {
    for(T* node = head; node; node = node->next) {
        if(node == entry) {
            return true;
        }
    }
    return false;
}

template<class T>
    requires IsListNode<T> || IsDListNode<T>
constexpr size_t ListSize(T* head) {
    size_t out = 0;
    for(T* node = head; node; node = node->next) {
        ++out;
    }
    return out;
}

// Remove a node from double linked list
template<class T>
    requires IsDListNode<T>
constexpr void DListRemove(T*& head, T* node) {
    if(node->prev) {
        node->prev->next = node->next;
    }
    if(node->next) {
        node->next->prev = node->prev;
    }
    if(head == node) {
        head = node->next;
    }
    node->next = nullptr;
    node->prev = nullptr;
}

template<class T>
    requires IsDListNode<T>
constexpr void DListPush(T*& head, T* node) {
    node->next = head;
    if(head) {
        head->prev = node;
    }
    head = node;
}

template<class T>
    requires IsDListNode<T>
constexpr T* DListPop(T*& head) {
    if(!head) {
        return nullptr;
    }
    T* oldHead = head;
    head = head->next;
    if(head) {
        head->prev = nullptr;
    }
    oldHead->next = nullptr;
    return oldHead;
}

template<class T>
    requires IsDListNode<T>
constexpr void ListDelete(T*& head) {
    while(T* node = DListPop(head)) {
        delete node;
    }
    head = nullptr;
}

template<class T>
    requires IsListNode<T> || IsDListNode<T>
constexpr auto ListIterate(T* node) {

    struct ListIterator {

        constexpr ListIterator(T* node): node(node) {}

        struct iterator {

            constexpr iterator() = default;
            
            constexpr iterator(T* node): node(node) {}

            constexpr iterator& operator++() { 
                node = node->next;
                return *this;
            }

            constexpr iterator operator++(int) {
                iterator tmp = *this;
                this->operator++();
                return tmp;
            }

            constexpr T* operator*() const {
                return node;
            }

            constexpr T* operator->() const {
                return node;
            }

            constexpr bool operator==(const iterator& right) const {
                return node == right.node;
            }

            T* node = nullptr;
        };

        iterator begin() { return {node}; }
        iterator end() { return {}; }

        T* node = nullptr;
    };

    return ListIterator{node};
}


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

inline bool WStringToString(std::wstring_view wstr, std::string& str) {
    size_t strLen;
    errno_t err;
    if ((err = wcstombs_s(&strLen, nullptr, 0, wstr.data(), _TRUNCATE)) != 0) {
        return false;
    }
    DASSERT(strLen > 0);
    // Remove \0
    str.clear();
    str.resize(strLen);
    // Remove \0
    str.pop_back();

    if ((err = wcstombs_s(nullptr, &str.front(), strLen, wstr.data(),
                          _TRUNCATE)) != 0) {
        return false;
    }
    return true;
}

class CommandLine {
public:

    static void Set(int argc, char* argv[]) {
        DASSERT(line_.empty());
        line_ = {argv, argv + argc};
        if(line_.size() > 1) {
            args_ = std::span<std::string>(line_.begin() + 1, line_.end());
        }
    }

    static void Set(int argc, wchar_t* argv[]) {
        DASSERT(line_.empty());
        for(int i = 0; i < argc; ++i) {
            std::wstring_view arg(argv[i]);
            std::string& elem = line_.emplace_back();
            WStringToString(arg, elem);
        }
        if(line_.size() > 1) {
            args_ = std::span<std::string>(line_.begin() + 1, line_.end());
        }
    }

    static void Set(std::convertible_to<std::string> auto ...args) {
        DASSERT(line_.empty());
        (line_.push_back(args), ...);
        if(line_.size() > 1) {
            args_ = std::span<std::string>(line_.begin() + 1, line_.end());
        }
    }

    static std::filesystem::path GetWorkingDir() {
        DASSERT(!line_.empty());
        return std::filesystem::path(line_.front()).parent_path();
    }

    static size_t ArgSize() { return args_.size(); }

    // Try to parse the arguments array
    // E.g. pares the line: program -arg_name arg_value
    template <class T>
        requires std::integral<T> || std::floating_point<T> ||
                 std::same_as<T, std::string>
    static std::optional<T> ParseArg(std::string_view argName) {
        std::string value;
        if(args_.empty() || args_.size() == 1) {
            return {};
        }
        for(auto it = args_.begin(); it != args_.end(); ++it) {
            if(*it != argName || (it + 1) == args_.end()) {
                continue;
            }
            value = *(it + 1);
            break;
        }
        if(value.empty()) {
            return {};
        }
        if constexpr(std::same_as<T, std::string>) {
            return value;
        } else {
            T parsedValue;
            const std::from_chars_result result = std::from_chars(
                value.data(), value.data() + value.size(), parsedValue, 10);
            if(result.ec != std::errc()) {
                return {};
            }
            return parsedValue;
        }
    }

    template <class T>
        requires std::integral<T> || std::floating_point<T> ||
                 std::same_as<T, std::string>
    static T ParseArgOr(std::string_view argName, const T& defaultValue) {
        std::optional<T> result = ParseArg<T>(argName);
        if(!result) {
            return defaultValue;
        }
        return result.value();
    }

    static std::string GetLine() {
        std::string out;
        for(const std::string& elem: line_) {
            out += elem + ' ';
        }
        return out;
    }

private:
    static inline std::vector<std::string> line_;
    static inline std::span<std::string> args_;
};


class Duration {
public:
    using duration_type = std::chrono::nanoseconds;

    Duration() = default;

    Duration(std::chrono::nanoseconds ns)
        : nanos(ns)
    {}

    static Duration Seconds(float secs) {
        Duration out;
        out.nanos = std::chrono::nanoseconds(static_cast<uint64_t>(secs * 1e9));
        return out;
    }
    
    static Duration Millis(float millis) {
        Duration out;
        out.nanos = std::chrono::nanoseconds(static_cast<uint64_t>(millis * 1e6));
        return out;
    }

    static Duration Micros(float micros) {
        Duration out;
        out.nanos = std::chrono::nanoseconds(static_cast<uint64_t>(micros * 1e6));
        return out;
    }

    std::chrono::microseconds ToMicros() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(nanos);
    }

    std::chrono::milliseconds ToMillis() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(nanos);
    }

    std::chrono::seconds ToSeconds() const {
        return std::chrono::duration_cast<std::chrono::seconds>(nanos);
    }

    float ToMicrosFloat() const {
        return nanos.count() / 1000.f;
    }

    float ToMillisFloat() const {
        return nanos.count() / 1'000'000.f;
    }

    float ToSecondsFloat() const {
        return nanos.count() / 1'000'000'000.f;
    }

    constexpr operator duration_type() const { return nanos; }

private:
    duration_type nanos;
};

class TimePoint {
public:

    static TimePoint Now() { 
        TimePoint out;
        out.timePoint_ = std::chrono::high_resolution_clock::now();
        return out;
    }

    friend Duration operator-(const TimePoint& lhs, const TimePoint& rhs) {
        return {lhs.timePoint_ - rhs.timePoint_};
    }

private:
    std::chrono::high_resolution_clock::time_point timePoint_ = {};
};

/*
* Simple timer
* Wrapper around std::chrono
*/
template<typename T = std::chrono::milliseconds>
struct Timer {

    using value_type = std::chrono::milliseconds;

    Timer()
        : SetPointDuration(0)
        , StartTimePoint()
    {}
            
    void Reset(uint32_t inValue) {
        SetPointDuration = value_type(inValue);
        StartTimePoint = std::chrono::high_resolution_clock::now();
    }

    bool IsTicking() const { return SetPointDuration.count() != 0; }

    bool IsReady() const {
        return IsTicking() ? std::chrono::duration_cast<value_type>(
                                 std::chrono::high_resolution_clock::now() -
                                 StartTimePoint) >= SetPointDuration
                           : false;
    }

    void Clear() { SetPointDuration = value_type(0); }

    value_type SetPointDuration;
    std::chrono::high_resolution_clock::time_point StartTimePoint;
};



/*
* Simple write only archive class
* Used for building a tree of objects with string properties
*/
class PropertyArchive {
public:

    struct Object;

    using ObjectID = uintptr_t;

    using Visitor = std::function<bool(Object&)>;

    struct Property {

        Property(std::string_view name, const std::string& value)
            : Name(name)
            , Value(value) {}

        std::string_view	Name;
        std::string			Value;
    };

    struct Object {

        Object(const std::string& className, ObjectID objectID, Object* parent)
            : debugName_(className)
            , objectID_(objectID) 
            , parent_(parent)
        {}

        void PushProperty(std::string_view name, const std::string& value) {
            properties_.emplace_back(name, value);
        }

        Object* EmplaceChild(const std::string& className, ObjectID objectID) {
            return  &*children_.emplace_back(new Object(className, objectID, this));
        }

        bool Visit(const Visitor& visitor) {
            auto shouldContinue = visitor(*this);
            if(!shouldContinue) return false;

            for(auto& child : children_) { 
                shouldContinue = child->Visit(visitor);
                if(!shouldContinue) return false;
            }
            return true;
        }

        ObjectID			objectID_ = 0;
        std::string			debugName_;
        std::list<Property> properties_;
        Object*				parent_ = nullptr;
        std::list<std::unique_ptr<Object>>	children_;
    };

public:

    // Visit objects recursively in depth first
    // Stops iteration if visitor returns false
    void VisitRecursively(const Visitor& visitor) {
        if(rootObjects_.empty()) return;

        for(auto& child : rootObjects_) { 
            const auto shouldContinue = child->Visit(visitor);
            if(!shouldContinue) return;
        }
    }

    template<class T>
        requires (std::is_pointer_v<T> || std::is_integral_v<T>)
    void PushObject(const std::string& debugName, T object, T parent) {
        const auto objectID = (ObjectID)object;
        const auto parentID = (ObjectID)parent;
        if(!parent || !cursorObject_) {
            cursorObject_ = &*rootObjects_.emplace_back(new Object(debugName, objectID, nullptr));
            return;
        }
        if(cursorObject_->objectID_ != parentID) {
            for(Object* ancestor = cursorObject_->parent_; ancestor; ancestor = ancestor->parent_) {
                if(ancestor->objectID_ == parentID) {
                    cursorObject_ = ancestor;
                }
            }
        }
        DASSERT(cursorObject_);
        cursorObject_ = cursorObject_->EmplaceChild(debugName, objectID);
    }

    // Push formatter property string
    template<class Vec2>
        requires (requires (Vec2 v) { v.x; v.y; })
    void PushProperty(std::string_view name, const Vec2& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, std::format("{:.0f}:{:.0f}", property.x, property.y));
    }

    void PushProperty(std::string_view name, const std::string& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, std::format("{}", property));
    }
    
    void PushStringProperty(std::string_view name, const std::string& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, std::format("\"{}\"", property));
    }

    template<typename Enum> 
        requires std::is_enum_v<Enum>
    void PushProperty(std::string_view name, Enum property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, ToString(property));
    }

    template<typename T> 
        requires std::is_arithmetic_v<T>
    void PushProperty(std::string_view name, T property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());

        if constexpr(std::is_integral_v<T>) {
            cursorObject_->PushProperty(name, std::format("{}", property));
        } else if constexpr(std::is_same_v<T, bool>) {
            cursorObject_->PushProperty(name, property ? "True" : "False");
        } else {
            cursorObject_->PushProperty(name, std::format("{:.2f}", property));
        }
    }

public:
    std::list<std::unique_ptr<Object>>	rootObjects_;
    Object*								cursorObject_ = nullptr;
};


// Calls the global function |Func| on object deletion
template<auto Func>
struct Deleter {
    template <class... Args>
    void operator()(Args&&... args) const {
        Func(std::forward<Args>(args)...);
    }
};


// Invoke a function with bound arguments on destruction
// Arguments should be copyable
template <class F, class... Args>
struct InvokeOnDestruct {
    InvokeOnDestruct(F&& func, Args... args) {
        fn = std::bind_back(std::forward<F>(func), std::forward<Args>(args)...);
    }

    ~InvokeOnDestruct() { 
        fn();
    }
    std::move_only_function<void()> fn;
};

// std::unique_ptr + operator& for C-style object construction
// E.g. Error err = CreateObject(&objectPtr);
template <class T, class Deleter = std::default_delete<T>>
class AutoFreePtr {
public:
    AutoFreePtr(AutoFreePtr&&) = delete;
    AutoFreePtr& operator=(AutoFreePtr&&) = delete;

    T* release() {
        T* out = ptr;
        ptr = nullptr;
        return out;
    }

    constexpr T** operator&() { return &ptr; }
    constexpr T& operator*() { return *ptr; }
    constexpr T* operator->() { return ptr; }

    constexpr operator T*() { return ptr; }

    AutoFreePtr() = default;
    ~AutoFreePtr() { Deleter()(ptr); }

    T* ptr = nullptr;
};



// Defines a range of elements of type V
// std::vector<V>, std::array<3, V>, std::string<V>
template <class T, class V>
concept Range =
    std::ranges::range<T> && std::convertible_to<typename T::value_type, V>;

