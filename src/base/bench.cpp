#include "bench.h"
#include <Windows.h>

namespace bench {

double Benchmark::GetCPUTimeSecondsDouble() {
    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;
    GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime,
                    &userTime);
    ULARGE_INTEGER kernel;
    ULARGE_INTEGER user;
    kernel.HighPart = kernelTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;
    user.LowPart = userTime.dwLowDateTime;
    return (static_cast<double>(kernel.QuadPart) +
            static_cast<double>(user.QuadPart)) *
            1e-7;
}

} // namespace bench
