#pragma once
#include <string>
#include <chrono>
#include <filesystem>

#include "math_util.h"

constexpr Vec4 HashColorToVec4(std::string_view inHexCode) {
	DASSERT(!inHexCode.empty() && inHexCode.starts_with('#') && (inHexCode.size() == 7 || inHexCode.size() == 9));
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

	/*
	* Helper to create string of serialized data
	*/
	class StringBuilder {
	public:

		constexpr StringBuilder(std::string* inBuffer, uint32_t inIndentSize = 2)
			: buffer_(inBuffer)
			, indent_(0)
			, indentSize_(inIndentSize)
		{}

        template <typename... ArgTypes>
        constexpr StringBuilder& Line(const std::format_string<ArgTypes...> inFormat,
            						  ArgTypes&&... inArgs) {
            AppendIndent();
			buffer_->append(std::format(inFormat, std::forward<ArgTypes>(inArgs)...));
			EndLine();
			return *this;
        }

        constexpr StringBuilder& Line(std::string_view inStr) {
			AppendIndent();
			buffer_->append(inStr);
			EndLine();
			return *this;
		}

		constexpr StringBuilder& Line() {
			EndLine();
			return *this;
		}

		constexpr StringBuilder& SetIndent(uint32_t inIndent = 1) {
			indent_ = inIndent;
			return *this;
		}

		constexpr StringBuilder& PushIndent(uint32_t inIndent = 1) {
			indent_ += inIndent;
			return *this;
		}

		constexpr StringBuilder& PopIndent(uint32_t inIndent = 1) {
			if(indent_) indent_ -= inIndent;
			return *this;
		}

		constexpr StringBuilder& EndLine() {
			buffer_->append("\n");
			return *this;
		}

	private:

		constexpr void AppendIndent() {
			if(!indent_) return;
			for(auto i = indent_ * indentSize_; i; --i) {
				buffer_->append(" ");
			}
		}

	private:
		uint32_t			 indentSize_;
		uint32_t			 indent_;
		std::string* buffer_;
	};

	inline std::wstring ToWideString(const std::string& inStr) {
		auto out = std::wstring(inStr.size() + 1, L'\0');
		size_t convertedChars = 0;
		mbstowcs_s(&convertedChars, out.data(), out.size(), inStr.c_str(), _TRUNCATE);
		return out;
	}

	inline std::string ToLower(const std::string& inString) {
		std::string str(inString);
		std::transform(
			std::begin(str),
			std::end(str),
			std::begin(str),
			[](unsigned char c) {
				return std::tolower(c);
			}
		);
		return str;
	}



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


template<class T>
concept CanBeRefCounted = requires (T* ptr) {
	ptr->AddRef();
	ptr->Release();
};

// Calls AddRef() and Release()
// Similar to boost::intrusive_ptr
template<class T>
	requires CanBeRefCounted<T>
class RefCountedPtr {
public:

	template<class U>
		requires CanBeRefCounted<U>
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

	constexpr T* operator->() const { return ptr; }

	constexpr operator T() const { return ptr; }

	constexpr T* Get() const { return ptr; }

	constexpr operator bool() const { return !!ptr; }

	// For windows IID_PPV
	constexpr T** operator& () { return &ptr; }

	void Swap(RefCountedPtr& rhs) { std::swap(ptr, rhs.ptr); }

private:
	T* ptr;
};

template<class T1, class T2>
constexpr bool operator==(const RefCountedPtr<T1>& left, const RefCountedPtr<T2>& right) {
    return left.Get() == right.Get();
}

template <class T1, class T2>
constexpr std::strong_ordering operator<=>(const RefCountedPtr<T1>& left, const RefCountedPtr<T2>& right) {
    return left.Get() <=> right.Get();
}





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



class CommandLine {
public:

	static void Set(int argc, char* argv[]) {
		args_ = {argv, argv + argc};
	}

	static std::filesystem::path GetWorkingDir() {
		DASSERT(!args_.empty());
		return std::filesystem::path(args_.front()).parent_path();
	}

private:
	static inline std::vector<std::string> args_;
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
        return IsTicking() ? 
            std::chrono::duration_cast<value_type>(std::chrono::high_resolution_clock::now() - StartTimePoint) >= SetPointDuration : 
            false;
    }

    void Clear() { SetPointDuration = value_type(0); }

    value_type SetPointDuration;
    std::chrono::high_resolution_clock::time_point StartTimePoint;
};