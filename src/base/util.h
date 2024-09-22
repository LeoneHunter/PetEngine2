#pragma once
#include "command_line.h"
#include "intrusive_list.h"
#include "math_util.h"
#include "ref_counted.h"
#include "string_utils.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <span>



class Duration {
public:
    using duration_type = std::chrono::nanoseconds;

    Duration() = default;

    Duration(std::chrono::nanoseconds ns) : nanos(ns) {}

    static Duration Seconds(float secs) {
        Duration out;
        out.nanos = std::chrono::nanoseconds(static_cast<uint64_t>(secs * 1e9));
        return out;
    }

    static Duration Millis(float millis) {
        Duration out;
        out.nanos =
            std::chrono::nanoseconds(static_cast<uint64_t>(millis * 1e6));
        return out;
    }

    static Duration Micros(float micros) {
        Duration out;
        out.nanos =
            std::chrono::nanoseconds(static_cast<uint64_t>(micros * 1e6));
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

    float ToMicrosFloat() const { return nanos.count() / 1000.f; }

    float ToMillisFloat() const { return nanos.count() / 1'000'000.f; }

    float ToSecondsFloat() const { return nanos.count() / 1'000'000'000.f; }

    constexpr operator duration_type() const { return nanos; }

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
template <typename T = std::chrono::milliseconds>
struct Timer {

    using value_type = std::chrono::milliseconds;

    Timer() : SetPointDuration(0), StartTimePoint() {}

    void Reset(uint32_t inValue) {
        SetPointDuration = value_type(inValue);
        StartTimePoint = std::chrono::high_resolution_clock::now();
    }

    bool IsTicking() const { return SetPointDuration.count() != 0; }

    bool IsReady() const {
        return IsTicking() ? std::chrono::duration_cast<value_type>(
                                 std::chrono::high_resolution_clock::now() -
                                 StartTimePoint) >= SetPointDuration
                           : false;
    }

    void Clear() { SetPointDuration = value_type(0); }

    value_type SetPointDuration;
    std::chrono::high_resolution_clock::time_point StartTimePoint;
};



// Calls the global function |Func| on object deletion
template <auto Func>
struct Deleter {
    template <class... Args>
    void operator()(Args&&... args) const {
        Func(std::forward<Args>(args)...);
    }
};


// Invoke a function with bound arguments on destruction
// Arguments should be copyable
template <class F, class... Args>
struct InvokeOnDestruct {
    InvokeOnDestruct(F&& func, Args... args) {
        fn = std::bind_back(std::forward<F>(func), std::forward<Args>(args)...);
    }

    ~InvokeOnDestruct() { fn(); }
    std::move_only_function<void()> fn;
};

// std::unique_ptr + operator& for C-style object construction
// E.g. Error err = CreateObject(&objectPtr);
template <class T, class Deleter = std::default_delete<T>>
class AutoFreePtr {
public:
    AutoFreePtr(AutoFreePtr&&) = delete;
    AutoFreePtr& operator=(AutoFreePtr&&) = delete;

    T* release() {
        T* out = ptr;
        ptr = nullptr;
        return out;
    }

    constexpr T** operator&() { return &ptr; }
    constexpr T& operator*() { return *ptr; }
    constexpr T* operator->() { return ptr; }

    constexpr operator T*() { return ptr; }

    AutoFreePtr() = default;
    ~AutoFreePtr() { Deleter()(ptr); }

    T* ptr = nullptr;
};

// Acii characters helper table
struct ASCII {
    enum Kind {
        D,  // Digit
        L,  // Letter
        P,  // Punctuation
        S,  // Space
        B,  // NewLine
        C,  // Control
    };

    // clang-format off
    static constexpr uint32_t table[] = {
    //                             t  CR t    LF
        C, C, C, C, C, C, C, C, C, S, B, S, B, B, C, C,
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
    // SPC !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /
        S, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P,
    //  0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
        D, D, D, D, D, D, D, D, D, D, P, P, P, P, P, P,
    //  @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
        P, L, L, L, L, L, L, L, L, L, L, L, L, L, L, L,
    //  P  Q  R  S  T  U  V  W  X  Y  Z  [  \\ ]  ^  _
        L, L, L, L, L, L, L, L, L, L, L, P, P, P, P, L,
    //  `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o
        P, L, L, L, L, L, L, L, L, L, L, L, L, L, L, L,
    //  p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~  7f
        L, L, L, L, L, L, L, L, L, L, L, P, P, P, P, C,
    };
    // clang-format on

    constexpr static bool IsSpace(char ch) { return table[ch] == S; }
    constexpr static bool IsLineBreak(char ch) { return table[ch] == B; }
    constexpr static bool IsLetter(char ch) { return table[ch] == L; }
    constexpr static bool IsDigit(char ch) { return table[ch] == D; }
    constexpr static bool IsPunctuation(char ch) { return table[ch] == P; }
    constexpr static bool IsControl(char ch) { return table[ch] == C; }
};
