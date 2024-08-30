#pragma once
#include "command_line.h"
#include "intrusive_list.h"
#include "math_util.h"
#include "string_utils.h"
#include "ref_counted.h"

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


/*
 * Simple write only archive class
 * Used for building a tree of objects with string properties
 */
class PropertyArchive {
public:
    struct Object;

    using ObjectID = uintptr_t;

    using Visitor = std::function<bool(Object&)>;

    struct Property {

        Property(std::string_view name, const std::string& value)
            : Name(name), Value(value) {}

        std::string_view Name;
        std::string Value;
    };

    struct Object {

        Object(const std::string& className, ObjectID objectID, Object* parent)
            : debugName_(className), objectID_(objectID), parent_(parent) {}

        void PushProperty(std::string_view name, const std::string& value) {
            properties_.emplace_back(name, value);
        }

        Object* EmplaceChild(const std::string& className, ObjectID objectID) {
            return &*children_.emplace_back(
                new Object(className, objectID, this));
        }

        bool Visit(const Visitor& visitor) {
            auto shouldContinue = visitor(*this);
            if (!shouldContinue)
                return false;

            for (auto& child : children_) {
                shouldContinue = child->Visit(visitor);
                if (!shouldContinue)
                    return false;
            }
            return true;
        }

        ObjectID objectID_ = 0;
        std::string debugName_;
        std::list<Property> properties_;
        Object* parent_ = nullptr;
        std::list<std::unique_ptr<Object>> children_;
    };

public:
    // Visit objects recursively in depth first
    // Stops iteration if visitor returns false
    void VisitRecursively(const Visitor& visitor) {
        if (rootObjects_.empty())
            return;

        for (auto& child : rootObjects_) {
            const auto shouldContinue = child->Visit(visitor);
            if (!shouldContinue)
                return;
        }
    }

    template <class T>
        requires(std::is_pointer_v<T> || std::is_integral_v<T>)
    void PushObject(const std::string& debugName, T object, T parent) {
        const auto objectID = (ObjectID)object;
        const auto parentID = (ObjectID)parent;
        if (!parent || !cursorObject_) {
            cursorObject_ = &*rootObjects_.emplace_back(
                new Object(debugName, objectID, nullptr));
            return;
        }
        if (cursorObject_->objectID_ != parentID) {
            for (Object* ancestor = cursorObject_->parent_; ancestor;
                 ancestor = ancestor->parent_) {
                if (ancestor->objectID_ == parentID) {
                    cursorObject_ = ancestor;
                }
            }
        }
        DASSERT(cursorObject_);
        cursorObject_ = cursorObject_->EmplaceChild(debugName, objectID);
    }

    // Push formatter property string
    template <class Vec2>
        requires(requires(Vec2 v) {
            v.x;
            v.y;
        })
    void PushProperty(std::string_view name, const Vec2& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(
            name, std::format("{:.0f}:{:.0f}", property.x, property.y));
    }

    void PushProperty(std::string_view name, const std::string& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, std::format("{}", property));
    }

    void PushStringProperty(std::string_view name,
                            const std::string& property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, std::format("\"{}\"", property));
    }

    template <typename Enum>
        requires std::is_enum_v<Enum>
    void PushProperty(std::string_view name, Enum property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());
        cursorObject_->PushProperty(name, ToString(property));
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    void PushProperty(std::string_view name, T property) {
        DASSERT_M(cursorObject_, "PushObject() should be called first");
        DASSERT(!name.empty());

        if constexpr (std::is_integral_v<T>) {
            cursorObject_->PushProperty(name, std::format("{}", property));
        } else if constexpr (std::is_same_v<T, bool>) {
            cursorObject_->PushProperty(name, property ? "True" : "False");
        } else {
            cursorObject_->PushProperty(name, std::format("{:.2f}", property));
        }
    }

public:
    std::list<std::unique_ptr<Object>> rootObjects_;
    Object* cursorObject_ = nullptr;
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

