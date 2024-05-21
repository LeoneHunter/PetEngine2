#include "core.h"

class Duration {
public:
    using duration_type = std::chrono::nanoseconds;

    static Duration Seconds(float secs) {
        Duration out;
        out.nanos = std::chrono::nanoseconds(static_cast<u64>(secs * 1e9));
        return out;
    }
    
    static Duration Millis(float millis) {
        Duration out;
        out.nanos = std::chrono::nanoseconds(static_cast<u64>(millis * 1e6));
        return out;
    }

    static Duration Micros(float micros) {
        Duration out;
        out.nanos = std::chrono::nanoseconds(static_cast<u64>(micros * 1e6));
        return out;
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
        return Duration::Micros((float)std::chrono::duration_cast<Duration::duration_type>(
                                lhs.timePoint_ - rhs.timePoint_).count());
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
            
    void Reset(u32 inValue) {
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