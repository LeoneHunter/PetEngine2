#include "task_executor.h"

thread_local TaskExecutor* currentThreadExecutor{};

TaskExecutor* TaskExecutor::GetForCurrentThread() {
    return currentThreadExecutor;
}

void TaskExecutor::RegisterTaskSource(std::shared_ptr<TaskSource> taskSource) {
    {
        std::scoped_lock _(lock_);
        auto it = std::ranges::find_if(
            sources_, 
            [&](auto&& e) { return e.get() == taskSource.get(); }
        );
        if(it != sources_.end()) {
            return;
        }
        sources_.push_back(taskSource);
        taskSource->OnExecutorSet(this);
    }
    if(!taskSource->Empty()) {
        ScheduleTaskSourceLocked(taskSource.get());
    }
}

void TaskExecutor::NotifyHasWork(TaskSource* source) {
    Assert(source);
    {
        std::scoped_lock _(lock_);
        auto it = std::ranges::find_if(
            sources_, 
            [&](const std::shared_ptr<TaskSource>& e) { 
                return e.get() == source; 
            }
        );
        Assertf(it != sources_.end(), "Invalid task source");
    }
    {
        std::scoped_lock _(lock_);
        auto it = std::ranges::find_if(
            readyQueue_,
            [&](const TaskSource* e) { return e == source; }
        );
        if(it != readyQueue_.end()) {
            return;
        }
    }
    ScheduleTaskSourceLocked(source);
}

void TaskExecutor::ScheduleTaskSourceLocked(TaskSource* source) {
    {
        std::scoped_lock _(lock_);
        readyQueue_.push_back(source);
    }
    semaphore_.release();
}

TaskSource::Handle TaskExecutor::TryOpenHandle() {
    std::scoped_lock _(lock_);
    if(readyQueue_.empty()) {
        return {};
    }
    for(;;) {
        TaskSource::Handle handle = readyQueue_.front()->OpenHandle();
        if(handle) {
            return std::move(handle);
        }
        // Empty if max concurrency for the task source is reached
        readyQueue_.pop_front();
        if(readyQueue_.empty()) {
            return {};
        }
    }
}

void TaskExecutor::Stop() {
    shouldExit.store(true, std::memory_order_relaxed);
    semaphore_.release(threadsNum_.load(std::memory_order_relaxed));
}

void TaskExecutor::RunUntilIdle() {
    Assert(tracker_);
    WorkerMain(false); 
}

void TaskExecutor::RunUntilStopped(bool canSleep) {
    Assert(tracker_);
    WorkerMain(canSleep);
}

void TaskExecutor::WorkerMain(bool canSleep) {
    Assert(tracker_);
    currentThreadExecutor = this;
    threadsNum_.fetch_add(1, std::memory_order_relaxed);

    for(;;) {
        if(shouldExit.load(std::memory_order_relaxed)) {
            return;
        }
        TaskSource::Handle handle = TryOpenHandle();
        // No work to do:
        if(!handle) {
            if(canSleep) {
                // Wait for signal
                semaphore_.acquire();
                continue;
            } else {
                return;
            }
        }
        // Process tasks accesible by the handle
        // The handle could be unique. A single thread uses a single provider
        // Or multiple threads use a single provider
        while(std::optional<Task> task = handle.TakeTask()) {
            Task::MetaInfo info = task->GetMetaInfo();
            tracker_->OnTaskStart(info);

            std::move(*task).Run();
            tracker_->OnTaskFinish(info);
        }
        handle.Close();
    }
    currentThreadExecutor = nullptr;
    threadsNum_.fetch_sub(1, std::memory_order_relaxed);
}



/*============================ THREAD POOL ============================*/
ThreadPool* defaultPool{};

void ThreadPool::SetDefault(ThreadPool* pool) {
    Assert(!defaultPool);
    defaultPool = pool;
}

ThreadPool& ThreadPool::GetDefault() { 
    Assert(defaultPool);
    return *defaultPool;
}

ThreadPool::ThreadPool(std::unique_ptr<TaskExecutor>&& executor,
                       uint64_t threadNum,
                       const std::string& threadNamePrefix)
    : executor_(std::move(executor)),
      namePrefix_(threadNamePrefix),
      threadNum_(threadNum) {
    Assert(executor_);
    if(threadNum_ == kThreadNumAuto) {
        threadNum_ = std::thread::hardware_concurrency();
    }
}

ThreadPool::ThreadPool(uint64_t threadNum, const std::string& threadNamePrefix)
    : executor_(), namePrefix_(threadNamePrefix), threadNum_(threadNum) {
    if(threadNum_ == kThreadNumAuto) {
        threadNum_ = std::thread::hardware_concurrency();
    }
    auto tracker = std::make_shared<LogTaskTracker>();
    executor_ = std::make_unique<TaskExecutor>(tracker);
}

void ThreadPool::Start() {
    threadsStartedEvent_ = std::make_unique<std::latch>(threadNum_);
    for(uint32_t i = 0; i < threadNum_; ++i) {
        std::string threadName = std::format("{} {}", namePrefix_, i);
        threads_.emplace_back(std::make_unique<Thread>(this, threadName));
        threads_.back()->Start();
    }
}

void ThreadPool::Stop() {
    if(threads_.empty()) {
        return;
    }
    executor_->Stop();
    for(auto& thread: threads_) {
        thread->Join();
    }
    threads_.clear();
}

void ThreadPool::WaitUntilStarted() {
    threadsStartedEvent_->wait();
    threadsStartedEvent_.reset();
}

void ThreadPool::RegisterTaskSource(std::shared_ptr<TaskSource> taskSource) {
    executor_->RegisterTaskSource(taskSource);
}

void ThreadPool::WorkerMain(bool canSleep) {
    threadsStartedEvent_->count_down();
    executor_->RunUntilStopped(canSleep);
}