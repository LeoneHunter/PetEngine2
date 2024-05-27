#pragma once
#include "runtime/core.h"
#include "runtime/time_utils.h"
#include "runtime/threading.h"

using TaskID = u64;



template <class F>
struct get_return_type {};

template <class R, class... A>
struct get_return_type<R (*)(A...)> {
    using type = R;
};

template <class R, class C, class... A>
struct get_return_type<R (C::*)(A...)> {
    using type = R;
};

template <class R, class C, class... A>
struct get_return_type<R (C::*)(A...) const> {
    using type = R;
};

template<class F>
using get_return_type_t = typename get_return_type<F>::type;


template<class T>
    requires std::is_pointer_v<T>
struct RawPointerWrapper {

    constexpr explicit RawPointerWrapper(T pointer)
        : pointer(pointer) 
    {}

    constexpr operator T() { return pointer; }

    T pointer = nullptr;
};

template<class T>
constexpr auto RawPointer(T pointer) {
    return RawPointerWrapper<std::decay_t<T>>(pointer);
}


template<class Func, class... Args>
concept CanBeCalled = requires (Func&& func, Args&&... args) {
    std::bind(std::forward<Func>(func), std::forward<Args>(args)...).operator()();
};


template<class T, class... Args>
    requires std::is_pointer_v<std::decay_t<T>> || 
             std::is_member_function_pointer_v<std::decay_t<T>>
struct IsValidFunction {
    using Func = std::decay_t<T>;

    static constexpr bool value = []{
        constexpr bool is_invocable = CanBeCalled<Func, std::decay_t<Args>...>;
        static_assert(is_invocable, 
                      "Function cannot be called with provided arguments. "
                      "Note: Move only types should be received by reference. "
                      "void MyFunc(std::unique_ptr<MyClass>& myInstance);");
        return is_invocable;
    }();
};

template<class T, class... Args>
    requires std::is_class_v<T>
struct IsValidLambda {
    using Func = std::decay_t<T>;

    static constexpr bool value = []{
        // constexpr bool is_captureless = std::is_empty_v<Func>;
        // static_assert(is_captureless, "Lambda should be captureless");
        constexpr bool is_invocable = CanBeCalled<Func, std::decay_t<Args>...>;
        static_assert(is_invocable, 
                      "Function cannot be called with provided arguments. "
                      "Note: Move only types should be received by reference. "
                      "void MyFunc(std::unique_ptr<MyClass>& myInstance);");
        return /*is_captureless &&*/ is_invocable;
    }();
};

template<class Func, class... Args>
concept IsValidCallable = 
    IsValidLambda<Func, Args...>::value || 
    IsValidFunction<Func, Args...>::value;

template<class Arg>
struct IsValidArgument {
    using type = std::decay_t<Arg>;

    static constexpr bool value = []{
        constexpr bool is_pointer = std::is_pointer_v<type>;
        static_assert(!is_pointer, 
                      "Raw pointers should be passed via RawPointer(arg)");
        return !is_pointer;
    }();
};

template<class... Args>
concept IsArgumentsValid = ((IsValidArgument<Args>::value) && ...);

// More strict std::bind for multithreaded environment
// Move only arguments should be passes as std::move(move_only)
// Raw pointers shoulc be passed with RawPointer(T* ptr)
// Other arguments are copied
// Captured lambdas are prohibited
template<class Func, class... Args>
    requires IsValidCallable<Func, Args...> && IsArgumentsValid<Args...>
auto BindStrict(Func&& func, Args&&... args) {
    return std::bind_front(std::forward<Func>(func), std::forward<Args>(args)...);
}


/**
 * Function + Arguments + Meta info
 * This is basically a "message" which is passed between threads,
 * so it should be thread safe, and arguments should be copied or moved
 * And raw pointers passing should be explicit
 * Because of that it should itself be only movable
*/
class Task {
public:

    // Meta info for debug
    struct MetaInfo {
        TaskID               id;
        TimePoint            timePoint;
        std::source_location location;
        std::string          postedThread;
    };

public:

    // TODO: Store binder in a Callback
    template<class Func, class... Args>
        requires IsValidCallable<Func, Args...> && IsArgumentsValid<Args...>
    static Task Bind(std::source_location location, Func&& func, Args&&... args) {
        auto out = Task(std::forward<Func>(func), std::forward<Args>(args)...);
        out.metaInfo_.id = math::RandomInteger<u64>();
        out.metaInfo_.location = location;
        out.metaInfo_.postedThread = Thread::GetCurrentThreadName();
        out.metaInfo_.timePoint = TimePoint::Now();
        return out;
    }

    // Signature: void()
    // For lambdas
    template<class Func>
        requires std::is_invocable_v<Func>
    Task(std::source_location location, Func&& func) 
        : callback_(std::move(func))
        , metaInfo_{
            .id = math::RandomInteger<u64>(),
            .timePoint = TimePoint::Now(),
            .location = location,
            .postedThread = Thread::GetCurrentThreadName(),
        }
    {}

    void Run() && {
        Assertf(!!callback_, "Callback is empty");
        std::move(callback_).operator()();
        callback_ = {};
    }

    const MetaInfo& GetMetaInfo() const { return metaInfo_; }

    bool Empty() const { return !callback_; }
    operator bool() const { return !!callback_; }

public:
    // Create with empty callable
    Task() = default;

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&&) = default;
    Task& operator=(Task&&) = default;

private:
    template<class Func, class... Args>
    Task(Func&& func, Args&&... args)
        : callback_(std::bind(std::forward<Func>(func),
                              std::forward<Args>(args)...))
    {}
    using Callable = std::move_only_function<void()&&>;

private:
	Callable callback_ = {};
	MetaInfo metaInfo_ = {};
};