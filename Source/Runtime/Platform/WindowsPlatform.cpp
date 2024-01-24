#include "WindowsPlatform.h"

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

#undef max
#undef min
#undef CreateSemaphore

namespace Windows {

	HANDLE CreateSemaphore(LONG lInitialCount, LONG lMaximumCount) {
		auto sem = CreateSemaphoreW(
			NULL,           // default security attributes
			lInitialCount,	// initial count
			lMaximumCount,	// maximum count
			NULL);          // unnamed semaphore

		if(sem == NULL) {
			//AssertFatal("Cannot create semaphore. Error code: {}", ::GetLastError());
			return NULL;
		}
		return sem;
	}

	PVOID GetCurrentFiber() {
		return ::GetCurrentFiber();
	}

	void Sleep(u32 inSleepTime) {
		::Sleep(1);
	}

	void Pause() {
		_mm_pause();
	}
}