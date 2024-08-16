#pragma once
#include "runtime/common.h"

#include <functional>

template<class F, class T = F::value_type>
concept IsFuture = 
    std::move_constructible<F> && 
    requires (F future, void(* func)(T)) {
        future.Then(func);
        { future.IsReady() } -> std::convertible_to<bool>;
        { future.GetValue() } -> std::convertible_to<T>;
    } || 
    std::is_void_v<T> &&
    requires (F future, void(* func)()) {
        future.Then(func);
        { future.IsReady() } -> std::convertible_to<bool>;
    };


namespace internal {
    
template<class T>
struct CallbackType {
    using type = std::move_only_function<void(T&&)>;

    constexpr static void Call(type&& func, std::optional<T>& arg) {
        (void) std::invoke(std::move(func), std::move(arg.value()));
    }

    template<class Func>
    constexpr static auto Bind(Func&& callback) {
        return [callback = std::move(callback)](T&& value) mutable {
            (void)std::invoke(std::move(callback), std::move(value));
        };
    }
};

template<>
struct CallbackType<void> {
    using type = std::move_only_function<void()>;

    constexpr static void Call(type&& func, std::optional<int>&) {
        (void) std::invoke(std::move(func));
    }

    template<class Func>
    constexpr static auto Bind(Func&& callback) {
        return [callback = std::move(callback)]() mutable {
            (void)std::invoke(std::move(callback));
        };
    }
};

} // namespace internal

// A state object stored on the heap
// Transfers the result of the asynchronous operation from
// Promise to Future
template<class T>
class SharedState {
public:

    // Use int as a placeholder if T is void
    using storage_type = std::conditional_t<std::is_void_v<T>, int, T>;
    using callback_type = internal::CallbackType<T>::type;

    enum class State: uint8_t {
        Pending,
        PendingWithCallback,
        Ready,
        Error,
        Finished
    };
    static_assert(std::atomic<State>::is_always_lock_free);

    SharedState(const SharedState&) = delete;
    SharedState operator=(const SharedState&) = delete;

    SharedState(SharedState&&) = delete;
    SharedState operator=(SharedState&&) = delete;

    SharedState()
        : state_(State::Pending) 
    {}

    bool IsReady() const {
        return state_.load(std::memory_order_acquire) == State::Ready;
    }

    bool IsFinished() const {
        return state_.load(std::memory_order_relaxed) == State::Finished;
    }

    storage_type GetValue() { 
        DASSERT(state_.load(std::memory_order_acquire) == State::Ready);
        DASSERT(value_);
        return std::move(value_).value();
    }

    template<class... Args>
        requires std::constructible_from<storage_type, Args...>
    void EmplaceValue(Args&&... args) {
        value_.emplace(std::forward<Args>(args)...);
        State old = state_.exchange(State::Ready, std::memory_order_acq_rel);
        DASSERT(old < State::Ready);

        if(old == State::PendingWithCallback) {
            internal::CallbackType<T>::Call(std::move(callback_), value_);
            state_.store(State::Finished, std::memory_order_relaxed);
        }
    }

    template<class Func>
    void SetCallback(Func&& callback) {
        DASSERT(!callback_);

        if(IsReady()) {
            DASSERT(value_);
            internal::CallbackType<T>::Call(std::move(callback), value_);
            state_.store(State::Finished, std::memory_order_relaxed);
            return;
        }
        callback_ = internal::CallbackType<T>::Bind(std::move(callback));
        state_.exchange(State::PendingWithCallback, std::memory_order_release);
    }

private:
    std::atomic<State>          state_;
    callback_type               callback_;
    std::optional<storage_type> value_;
};

// Similar to std::future
// Represents a result of an asynchronous operation
// Can be waited upon or can register a callback
template<class T>
class Future {
public:
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_array_v<T>);

    using value_type = T;

    Future(std::shared_ptr<SharedState<T>> state)
        : state_(state)
    {}

    Future(Future&& rhs) = default;

    Future& operator=(Future&& rhs) {
        Future(std::move(rhs)).Swap(*this);
        return *this;
    }

    void Swap(Future& rhs) {
        std::swap(state_, rhs.state_);
    }

    bool IsReady() const {
        return state_->IsReady();
    }

    bool IsFinished() const {
        return state_->IsFinished();
    }

    T GetValue() {
        DASSERT(state_->IsReady());
        return std::move(state_->GetValue());
    }

    template<class Func>
        requires std::invocable<Func, T> || (std::is_void_v<T> && std::invocable<Func>)
    void Then(Func&& callback) {
        state_->SetCallback(std::move(callback));
    }

    bool Empty() const { return !state_; }

private:
    std::shared_ptr<SharedState<T>> state_;
};

template<>
class Future<void> {
public:

    using value_type = void;

    Future(std::shared_ptr<SharedState<void>> state)
        : state_(state)
    {}

    Future(Future&& rhs) = default;

    Future& operator=(Future&& rhs) {
        Future(std::move(rhs)).Swap(*this);
        return *this;
    }

    void Swap(Future& rhs) {
        std::swap(state_, rhs.state_);
    }

    bool IsReady() const {
        return state_->IsReady();
    }

    bool IsFinished() const {
        return state_->IsFinished();
    }

    template<class Func>
        requires std::invocable<Func>
    void Then(Func&& callback) {
        state_->SetCallback(std::move(callback));
    }

    bool Empty() const { return !state_; }

private:
    std::shared_ptr<SharedState<void>> state_;
};

// Similar to std::promise
template<class T>
class Promise {
public:

    Promise()
        : state_(new SharedState<T>())
    {}

    Promise(Promise&& rhs) = default;

    Promise& operator=(Promise&& rhs) {
        Promise(std::move(rhs)).Swap(*this);
        return *this;
    }

    void Swap(Promise& rhs) {
        std::swap(hasFuture_, rhs.hasFuture_);
        std::swap(state_, rhs.state_);
    }

    void Resolve(T&& val) {
        state_->EmplaceValue(std::forward<decltype(val)>(val));
    }

    Future<T> GetFuture() {
        DASSERT_F(!hasFuture_, 
               "Future for this promise has already been created");
        hasFuture_ = true;
        return {state_};
    }

    bool IsFinished() const { return state_->IsFinished(); }

    bool Empty() const { return !state_; }

private:
    bool hasFuture_ = false;
    std::shared_ptr<SharedState<T>> state_;
};

template<>
class Promise<void> {
public:

    Promise()
        : state_(new SharedState<void>())
    {}

    Promise(Promise&& rhs) = default;

    Promise& operator=(Promise&& rhs) {
        Promise(std::move(rhs)).Swap(*this);
        return *this;
    }

    void Swap(Promise& rhs) {
        std::swap(hasFuture_, rhs.hasFuture_);
        std::swap(state_, rhs.state_);
    }

    void Resolve() {
        state_->EmplaceValue(1);
    }

    Future<void> GetFuture() {
        DASSERT_F(!hasFuture_, 
               "Future for this promise has already been created");
        hasFuture_ = true;
        return {state_};
    }

    bool IsFinished() const { return state_->IsFinished(); }

    bool Empty() const { return !state_; }

private:
    bool hasFuture_ = false;
    std::shared_ptr<SharedState<void>> state_;
};