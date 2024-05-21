#include "threading.h"
#include "windows_platform.h"
#include "util.h"

namespace thread {

thread_local std::string currentThreadName;

void SetCurrentThreadName(const std::string& name) {
    currentThreadName = name;
    auto wname = util::ToWideString(name);
    windows::SetThreadDescription(windows::GetCurrentThread(), wname.c_str());
}

const std::string& GetCurrentThreadName() {
    return currentThreadName;
}

ThreadID GetCurrentThreadID() {
    return windows::GetCurrentThreadId();
}

} // namespace thread

