#pragma once
#include "task.h"
#include "future.h"

#include "base/threading.h"

#include <queue>
#include <coroutine>

class TaskExecutor;
class TaskTracker;

template<class T>
class EventLoopFuture;

template<class T>
class EventLoopPromise;

// Abstraction aver task source
// Could be TaskQueue or TaskGenerator for parallel algorithms
// Could be even a TaskGraph with predetermined dependencies between tasks
class TaskSource {
public:

    // A task can be retrieved only while holding this handle
    // Ensures max concurrency is met and task execution is synchronized
    // Shouldn't be destroyed until task is finished
    // Can be reused by a thread (e.g. for processing a single event loop)
    // Can be returned to multiple threads if max concurrency is not reached
    class Handle {
    public:
        Handle(Handle&& rhs) { std::swap(taskSource_, rhs.taskSource_); }
        Handle& operator=(Handle&& rhs) { std::swap(taskSource_, rhs.taskSource_); return *this; }

        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;

        // Creates an empty handle
        Handle() = default;
        ~Handle() { Close(); }

        void Close() {
            if(!Empty()) {
                taskSource_->CloseHandle();
                taskSource_ = nullptr;
            }
        }

        // Takes task from the taskSource queue
        // Could return Null if no more work to do
        // Optional explicitly shows that it's an empty optional or a valid task
        std::optional<Task> TakeTask() {
            if(Empty()) {
                return {};
            }
            Task out = taskSource_->TakeTask(this);
            if(!out) {
                return {};
            }
            return std::move(out);
        }

        bool Empty() const { return taskSource_ == nullptr; }
        operator bool() const { return !Empty(); }
    
    private:
        Handle(TaskSource* taskSource) 
            : taskSource_(taskSource) 
        {}
        friend class TaskSource;

    private:
		TaskSource* taskSource_{};
	};

    // Returns an empty handle if:
    // - Max concurrency is reached
    // - Shutdown in progress
    // - No work to do
    virtual Handle OpenHandle() = 0;

    virtual void OnExecutorSet(TaskExecutor* executor) = 0;
    virtual bool Empty() const = 0;

protected:
    Handle CreateHandle() { return {this}; }

private:
    Task TakeTask(Handle* handle) {
        DASSERT(handle->taskSource_ == this);
        return TakeTask();
    }

    virtual void CloseHandle() = 0;
    virtual Task TakeTask() = 0;
};

// Checks whether a Callback can be called with the Func result
template<class Func, class Callback>
concept CallbackFor = std::invocable<Callback, std::invoke_result_t<Func>> || 
                     (std::is_void_v<std::invoke_result_t<Func>> && std::invocable<Func> && 
                      std::invocable<Callback>);


// A container for tasks
// Tasks executed sequentually 
// TODO: Add delayed tasks
// TODO: Add priorities
// TODO: Add cancelation
class EventLoop final: 
    public TaskSource, 
    public std::enable_shared_from_this<EventLoop> {
public:
    // An event loop currently executing on this thread
    static std::shared_ptr<EventLoop> GetForCurrentThread();

    template<class Func>
        requires std::invocable<Func>
    void PostTask(Func&& func, std::source_location location = std::source_location::current()) {
        PostTaskInternal(Task(location, std::forward<Func>(func)));
    }

    // Posts a task and a callback which will be posted on the provided event loop
    template<class Func, class Callback>
        requires CallbackFor<Func, Callback>  
    void PostTaskWithCallbackOn(std::shared_ptr<EventLoop> target,
                                Func&&                     func,
                                Callback&&                 callback,
                                std::source_location       location = std::source_location::current()) {

        DASSERT_F(target, "Provided target EventLoop should not be empty");
        using result_type = std::invoke_result_t<Func>;

        auto task = Task(
            location,
            [target   = target,
             location = location,
             callback = std::move(callback),
             func     = std::move(func)]() mutable {
                std::shared_ptr<EventLoop> current = EventLoop::GetForCurrentThread();
                DASSERT(current);

                if constexpr(std::is_void_v<result_type>) {
                    std::invoke(std::move(func));
                    if(target == current) {
                        std::invoke(std::move(callback));
                    } else {
                        target->PostTask(std::move(callback), location);
                    }
                } else {
                    auto result = std::invoke(std::move(func));

                    if(target == current) {
                        std::invoke(std::move(callback), std::move(result));
                    } else {
                        target->PostTask(
                            [callback = std::move(callback), result = std::move(result)]() mutable {
                                std::invoke(std::move(callback), std::move(result));
                            }, 
                            location
                        );
                    }
                }
            }
        );
        PostTaskInternal(std::move(task));
    }

    // Posts a task and a callback which will be posted on the current event loop
    template<class Func, class Callback>
    void PostTaskWithCallback(Func&&               func,
                              Callback&&           callback,
                              std::source_location location = std::source_location::current()) {
        return PostTaskWithCallbackOn(
            EventLoop::GetForCurrentThread(),
            std::forward<Func>(func),
            std::forward<Callback>(callback),
            location
        );
    }

    // Posts a task and returns a Future that can be co_await-ed by a coroutine
    // Or queried for a completion
    template<class Func>
        requires std::invocable<Func>
    auto PostTaskWithPromise(Func&&               func,
                             std::source_location location = std::source_location::current()) {
        using result_type = std::invoke_result_t<Func>;
        using promise_type = EventLoopPromise<result_type>;
        // Post a task on this event loop
        // Return a future with reference to this event loop
        auto promise = promise_type{};
        auto future = promise.GetFuture();
        auto callback = std::bind_front(&promise_type::Resolve, std::move(promise));

        PostTaskWithCallbackOn(
            shared_from_this(),
            std::forward<Func>(func),
            std::move(callback),
            location
        );
        return std::move(future);
    }

    Handle OpenHandle() override;
    bool Empty() const override;

public:
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop() = default;
    ~EventLoop() { DASSERT(!hasUser_); }

private:
	void PostTaskInternal(Task&& task);
    Task TakeTask() override;
    // Called on handle destruction
    void CloseHandle() override;
    void OnExecutorSet(TaskExecutor* executor) override;

private:
    // Ensures exclusive acces by a single thread
    mutable std::mutex           lock_;
    std::queue<Task>             tasks_;
    std::shared_ptr<TaskTracker> tracker_;
    // Executor owns us so this is a back ref
    TaskExecutor*                executor_ = nullptr;
    bool                         hasUser_ = false;
};



template<class T>
struct EventLoopCoroutine;

// A Future returned by EventLoop
// Resolved on an EventLoop
// Could be used with coroutines
template<class T>
class EventLoopFuture: public Future<T> {
public:
    using Base = Future<T>;
    // For coroutine support. "EventLoopFuture<void> MyCoroutine();"
    using promise_type = EventLoopCoroutine<T>;

    EventLoopFuture(Base&& base)
        : Base(std::move(base))
    {}

    template<class Func>
        requires std::invocable<Func, T> || (std::is_void_v<T> && std::invocable<Func>)
    auto ThenOnCurrent(Func&& onCompletion) {
        // Schefule a func on current event loop when ready
        std::shared_ptr<EventLoop> current = EventLoop::GetForCurrentThread();
        DASSERT(current);

        if constexpr(std::is_void_v<T>) {
            this->Then([target = current, onCompletion = std::move(onCompletion)]() mutable {
                std::shared_ptr<EventLoop> current = EventLoop::GetForCurrentThread();
                DASSERT(current);
                if (target == current) {
                    std::invoke(std::move(onCompletion));
                } else {
                    target->PostTask(std::move(onCompletion));
                }
            });
        } else {
            this->Then([target = current, onCompletion = std::move(onCompletion)](auto&& result) mutable {
                std::shared_ptr<EventLoop> current = EventLoop::GetForCurrentThread();
                DASSERT(current);
                if (target == current) {
                    std::invoke(std::move(onCompletion), std::move(result));
                } else {
                    target->PostTask(std::bind_front(std::move(onCompletion), std::move(result)));
                }
            });
        }
    }
};

// A Promise signalled by the task on completion
template<class T>
class EventLoopPromise: public Promise<T> {
public:
    using Base = Promise<T>;

    EventLoopFuture<T> GetFuture() {
        return Base::GetFuture();
    }
};




// Coroutine promise to be used with event loops
// Coroutine could be suspended awaiting for a Future<T>
template<class T>
struct EventLoopCoroutine {

    template<class F>
    struct Awaitable {
        using value_type = F::value_type;
        using storage_type = std::conditional_t<std::is_void_v<value_type>, int, value_type>;
        
        bool await_ready() noexcept { 
            if(future.IsReady()) {
                if constexpr(!std::is_void_v<value_type>) {
                    result.emplace(std::move(future.GetValue()));
                }
                return true;
            }
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) noexcept {
            std::shared_ptr<EventLoop> current = EventLoop::GetForCurrentThread();
            DASSERT(current);

            if constexpr(!std::is_void_v<value_type>) {
                future.Then([this, handle = handle, target = current](value_type&& result) {
                    this->result.emplace(std::move(result));
                    std::shared_ptr<EventLoop> current = EventLoop::GetForCurrentThread();
                    DASSERT(current);

                    if(current == target) {
                        handle.resume();
                    } else {
                        target->PostTask(std::bind_front(&std::coroutine_handle<>::resume, handle));
                    }
                });
            } else {
                future.Then([this, handle = handle, target = current]() {
                    std::shared_ptr<EventLoop> current = EventLoop::GetForCurrentThread();
                    DASSERT(current);

                    if(current == target) {
                        handle.resume();
                    } else {
                        target->PostTask(std::bind_front(&std::coroutine_handle<>::resume, handle));
                    }
                });
            }
        }

        // Return the result to the caller
        // After this function returns 'this' is destroyed
        value_type await_resume() noexcept {
            if constexpr(!std::is_void_v<value_type>) {
                DASSERT(result.has_value());
                return std::move(result).value();
            }
        }

        Awaitable(F&& future)
            : future(std::move(future))
        {}

        F                           future;
        std::optional<storage_type> result;
    };

    // Called when we co_await a Future
    template<class F>
        requires IsFuture<F>
    Awaitable<F> await_transform(F&& future) {
        return {std::move(future)};
    }

    EventLoopFuture<T> get_return_object() { 
        return promise.GetFuture();
    }

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() {}

    void return_void() {}

    EventLoopPromise<T> promise;
};

