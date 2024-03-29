#include <filesystem>
#include "runtime/core/types.h"
#include "job_dispatcher.h"

namespace Filesystem {

	using Path = std::filesystem::path;


}

template<class T>
struct DefaultDeleter {
	DefaultDeleter() = default;

	void Delete(T* inObject) {
		delete inObject;
	}
};

template<class T, class Deleter = DefaultDeleter<T>>
class RefCounted {
public:

	RefCounted() = default;
	virtual ~RefCounted() = default;

	void Incref() {
		referenceCount_.fetch_add(1, std::memory_order::relaxed);
	}

	void Decref() {
		const auto count = referenceCount_.fetch_sub(1, std::memory_order::relaxed);

		if(count == 1) {
			deleter_.Delete(this);
		}
	}

private:
	Deleter					deleter_;
	std::atomic_uint64_t	referenceCount_;
};

template<class T>
struct ReferenceCountedPtr {

	using ref_counted = T;

	ReferenceCountedPtr() = default;
	ReferenceCountedPtr(ref_counted* inRefcounter);
	~ReferenceCountedPtr();

	ReferenceCountedPtr(const ReferenceCountedPtr& other);
	ReferenceCountedPtr& operator= (const ReferenceCountedPtr& right);

	NODISCARD constexpr ref_counted& operator*() const {
		return *refCounted_;
	}

	NODISCARD constexpr ref_counted* operator->() const {
		return refCounted_;
	}

	constexpr operator bool() const { return refCounted_ != nullptr; }

public:
	ref_counted* refCounted_ = nullptr;
};

namespace IO {

	enum class StatusCode {
		COMPLETED,
		FILE_NOT_FOUND,
		PENDING
	};

	/* 
	* Async future
	*/
	struct Request: RefCounted<Request> {
		JobSystem::EventRef		completedEvent_;
		StatusCode				status_;
		std::vector<u8>			buffer_;
	};

	using RequestRef = ReferenceCountedPtr<Request>;

	// Reads file asynchronously
	RequestRef ReadAsync(const Filesystem::Path& inFilename, size_t inReadSize, size_t inOffset = 0);

}