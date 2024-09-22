#include <chrono>
#include <functional>

namespace bench {

// Simple benchmark runner
class Benchmark {
public:
    struct Stats {
        uint32_t numIters;
        struct {
            double total;
            double average;
            double min;
            double max;
        } wallTime;
        // NOTE: Cpu time on Windows has 15ms granularity
        struct {
            double total;
            double average;
            double min;
            double max;
        } cpuTime;
    };

public:
    void SetMain(const std::function<void()>& func) { func_ = func; }

    void Run(uint32_t iters) {
        iters = std::max(iters, 1U);
        stats_.numIters = iters;
        constexpr auto kWarmUpIters = 1;
        stats_.wallTime.min = std::numeric_limits<double>::max();
        stats_.cpuTime.min = std::numeric_limits<double>::max();

        for (uint32_t i = 0; i < iters + kWarmUpIters; ++i) {
            const double startWall = GetWallTimeSecondsDouble();
            const double startCPU = GetCPUTimeSecondsDouble();
            func_();
            const double endWall = GetWallTimeSecondsDouble();
            const double timeWall = std::max(endWall - startWall, 0.);

            const double endCPU = GetCPUTimeSecondsDouble();
            const double timeCPU = std::max(endCPU - startCPU, 0.);

            if (i >= kWarmUpIters) {
                stats_.wallTime.total += timeWall;
                stats_.wallTime.min = std::min(stats_.wallTime.min, timeWall);
                stats_.wallTime.max = std::max(stats_.wallTime.max, timeWall);

                if (timeCPU > 0.) {
                    stats_.cpuTime.total += timeCPU;
                    stats_.cpuTime.min = std::min(stats_.cpuTime.min, timeCPU);
                    stats_.cpuTime.max = std::max(stats_.cpuTime.max, timeCPU);
                }
            }
        }
        stats_.wallTime.average = stats_.wallTime.total / stats_.numIters;
        stats_.cpuTime.average = stats_.cpuTime.total / stats_.numIters;
    }

    Stats GetStats() const { return stats_; }

private:
    static double GetWallTimeSecondsDouble() {
        using Seconds =
            std::chrono::duration<double, std::chrono::seconds::period>;
        return Seconds(
                   std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    }

    static double GetCPUTimeSecondsDouble();

private:
    Stats stats_{};
    std::function<void()> func_;
};

// Simple timer
class Timer {
public:
    Timer() { start = std::chrono::high_resolution_clock::now(); }

    Timer& Stop() {
        if (!stopped) {
            finish = std::chrono::high_resolution_clock::now();
        }
        return *this;
    }

    float Millis() {
        Stop();
        return (float)(finish - start).count() / 1'000'000.f;
    }

    float Micros() {
        Stop();
        return (float)(finish - start).count() / 1'000.f;
    }

    float Nanos() {
        Stop();
        return (float)(finish - start).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point finish;
    bool stopped = false;
};

class ScopedTimer {
public:
    ScopedTimer(const std::function<void(Timer&)>& callback)
        : callback(callback) {
    }

    ~ScopedTimer() {
        tm.Stop();
        callback(tm);
    }

private:
    Timer tm;
    std::function<void(Timer&)> callback;
};

}  // namespace bench
