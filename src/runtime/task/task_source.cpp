#include "task_source.h"
#include "task_executor.h"
#include "task_tracker.h"

// An Event loop currently executing on the current thread
thread_local EventLoop* currentThreadEventLoop = nullptr;

std::shared_ptr<EventLoop> EventLoop::GetForCurrentThread() {
    if(!currentThreadEventLoop) {
        return nullptr;
    }
    return currentThreadEventLoop->shared_from_this();
}

void EventLoop::OnExecutorSet(TaskExecutor* executor) {
    DASSERT_F(!executor_, "Only one TaskExecutor can be assigned");
    DASSERT_F(!hasUser_, "Cannot change TaskExecutor while processing tasks");
    executor_ = executor;
}

TaskSource::Handle EventLoop::OpenHandle() {
    std::scoped_lock _(lock_);
    if(tasks_.empty() || hasUser_) {
        return {};
    }
    hasUser_ = true;
    currentThreadEventLoop = this;
    return CreateHandle();
}

void EventLoop::PostTaskInternal(Task&& task) {
    Task::MetaInfo info = task.GetMetaInfo();
    {
        std::scoped_lock _(lock_);
        tasks_.push(std::move(task));
    }
    if(!executor_) {
        return;
    }
    if(tracker_) {
        tracker_->OnTaskPost(info);
    }
    executor_->NotifyHasWork(this);
}

Task EventLoop::TakeTask() {
    std::scoped_lock _(lock_);
    if(tasks_.empty()) {
        return {};
    }
    Task out = std::move(tasks_.front());
    tasks_.pop();
    return out;
}

void EventLoop::CloseHandle() {
    hasUser_ = false;
    currentThreadEventLoop = nullptr;
}

bool EventLoop::Empty() const {
    std::scoped_lock _(lock_);
    return tasks_.empty();
}
