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
		m_ReferenceCount.fetch_add(1, std::memory_order::relaxed);
	}

	void Decref() {
		const auto count = m_ReferenceCount.fetch_sub(1, std::memory_order::relaxed);

		if(count == 1) {
			m_Deleter.Delete(this);
		}
	}

private:
	Deleter					m_Deleter;
	std::atomic_uint64_t	m_ReferenceCount;
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
		return *m_RefCounted;
	}

	NODISCARD constexpr ref_counted* operator->() const {
		return m_RefCounted;
	}

	constexpr operator bool() const { return m_RefCounted != nullptr; }

public:
	ref_counted* m_RefCounted = nullptr;
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
		JobSystem::EventRef		m_CompletedEvent;
		StatusCode				m_Status;
		std::vector<u8>			m_Buffer;
	};

	using RequestRef = ReferenceCountedPtr<Request>;

	// Reads file asynchronously
	RequestRef ReadAsync(const Filesystem::Path& inFilename, size_t inReadSize, size_t inOffset = 0);

}