#pragma once
#include "task.h"
#include "task_source.h"
#include "task_tracker.h"

#include "runtime/threading.h"

#include <latch>

// Abstraction between threads and task sources
// Schedules N task sources over M threads
// So that there could be a single thread processing multiple sources
// or a single source processed by multiple threads
// FIXME: Currently has simple scheduling logic: one thread per task source
class TaskExecutor : public Thread::Delegate {
public:
    static TaskExecutor* GetForCurrentThread();

    TaskExecutor(std::shared_ptr<TaskTracker> tracker) : tracker_(tracker) {}

    ~TaskExecutor() { Stop(); }

    // Register a task soutce to be processed by this executor
    // A client can hold the reference to the taskSource and continue to post
    // tasks to it Or the taskSource could be released and it will be deleted
    // when all tasks have been processed
    // TODO: Destroy the task taskSource if has only one reference, or reuse for
    // internal tasks
    void RegisterTaskSource(std::shared_ptr<TaskSource> taskSource);

    // Stops work scheduling based on TaskProvider's TaskShutdownPolicy
    void Stop();

    // Could be called from any thread to execute tasks from this executor
    void RunUntilIdle();

    // Could be called from any thread to execute tasks from this executor
    // Stop() should be called to return
    void RunUntilStopped(bool canSleep);

    void NotifyHasWork(TaskSource* source);

private:
    // TODO: Add deadline for testing
    // Entered by a worker
    void WorkerMain(bool canSleep) override;

    // Iterates over TaskSource's and tries to acquire a handle
    // Returns an empty handle if no more work
    TaskSource::Handle TryOpenHandle();

    // Adds to source to ready queue without validation
    void ScheduleTaskSourceLocked(TaskSource* source);

private:
    std::atomic_bool shouldExit;
    std::atomic_long threadsNum_{0};
    std::counting_semaphore<> semaphore_{0};
    std::shared_ptr<TaskTracker> tracker_;
    std::mutex lock_;
    // All task sources, some could be empty at the moment
    std::vector<std::shared_ptr<TaskSource>> sources_;
    // Task sources ready to be processed
    std::deque<TaskSource*> readyQueue_;
};

constexpr auto kThreadNumAuto = 0;

// Basic thread pool
// Each thread enters the TaskExecutor::WorkerMain and takes a queue
class ThreadPool: public Thread::Delegate {
private:
    // Sets the default pool to be used with static methods
    // Like: ThreadPool::PostTask();
    static void SetDefault(ThreadPool* pool);
    static ThreadPool& GetDefault();

public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(std::unique_ptr<TaskExecutor>&& executor,
               uint64_t threadNum = kThreadNumAuto,
               const std::string& threadNamePrefix = "Worker Thread");

    // Creates a pool with dedicated tracker and executor
    ThreadPool(uint64_t threadNum = kThreadNumAuto,
               const std::string& threadNamePrefix = "Worker Thread");

    ~ThreadPool() { Stop(); }

    void Start();
    void Stop();
    // Mostly for testing
    void WaitUntilStarted();
    void RegisterTaskSource(std::shared_ptr<TaskSource> taskSource);

private:
    void WorkerMain(bool canSleep = true) override;

private:
    uint64_t threadNum_{};
    std::string namePrefix_;
    std::unique_ptr<std::latch> threadsStartedEvent_;
    std::unique_ptr<TaskExecutor> executor_;
    std::vector<std::unique_ptr<Thread>> threads_;
};


// A thread with a TaskScheduler and an EventLoop
class DedicatedThread : public Thread::Delegate {
public:
    DedicatedThread(const std::string& name,
                    std::shared_ptr<TaskTracker> tracker)
        : thread_(this, name) {
        executor_ = std::make_unique<TaskExecutor>(tracker);
    }

    void Start() {
        thread_.Start();
        thread_.WaitUntilStarted();
    }

    void Stop() { executor_->Stop(); }

    void Join() { thread_.Join(); }

    std::shared_ptr<EventLoop> CreateEventLoop() {
        auto eventLoop = std::make_shared<EventLoop>();
        executor_->RegisterTaskSource(eventLoop);
        return eventLoop;
    }

private:
    void WorkerMain(bool) override { executor_->RunUntilStopped(true); }

private:
    Thread thread_;
    std::unique_ptr<TaskExecutor> executor_;
};