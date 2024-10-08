#pragma once
#include <stdint.h>

#ifndef WINAPI
	#define WINAPI __stdcall
#endif

#ifndef WINBASEAPI
	#define WINBASEAPI extern "C" __declspec(dllimport)
#endif

// A subset of Windows.h
namespace windows {

	using VOID = void;
	using LPVOID = void*;
	using PVOID = void*;
	using SIZE_T = uint64_t;
	using HANDLE = void*;
	using DWORD = unsigned long;
	using LONG = long;
	using LPLONG = LONG*;
	using CHAR = char;
	using BOOL = int;
	using HRESULT = long;
	using WCHAR = wchar_t;

	typedef const WCHAR* LPCWSTR, *PCWSTR;
	typedef VOID(WINAPI* LPFIBER_START_ROUTINE)(LPVOID lpFiberParameter);

	WINBASEAPI LPVOID WINAPI CreateFiber(SIZE_T dwStackSize, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter);
	WINBASEAPI LPVOID WINAPI ConvertThreadToFiber(LPVOID lpParameter);
	WINBASEAPI VOID WINAPI SwitchToFiber(LPVOID lpFiber);
	WINBASEAPI VOID WINAPI DeleteFiber(LPVOID lpFiber);
	WINBASEAPI DWORD WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
	WINBASEAPI BOOL WINAPI ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount);
	WINBASEAPI LPVOID WINAPI ConvertThreadToFiber(LPVOID lpParameter);
	WINBASEAPI HRESULT WINAPI SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription);
	WINBASEAPI HANDLE WINAPI GetCurrentThread(VOID);
	WINBASEAPI DWORD WINAPI GetCurrentThreadId(VOID);

	HANDLE CreateSemaphore(LONG lInitialCount, LONG lMaximumCount);
	PVOID GetCurrentFiber();	
	void Sleep(uint32_t inSleepTimeMs);
	void Pause();

	void SetConsoleCodepageUtf8();
}