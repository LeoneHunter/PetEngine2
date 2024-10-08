#include "threading.h"
#include "win_minimal.h"
#include "string_utils.h"

thread_local std::string currentThreadName;

void Thread::SetCurrentThreadName(const std::string& name) {
    currentThreadName = name;
    auto wname = ToWideString(name);
    windows::SetThreadDescription(windows::GetCurrentThread(), wname.c_str());
}

const std::string& Thread::GetCurrentThreadName() {
    return currentThreadName;
}

ThreadID Thread::GetCurrentThreadID() {
    return windows::GetCurrentThreadId();
}


