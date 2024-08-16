#include "task_tracker.h"
#include "runtime/threading.h"

// A task index currently executing on the current thread
thread_local std::optional<size_t> currentThreadTaskIndex;

void LogTaskTracker::OnTaskPost(const Task::MetaInfo& taskInfo) {
    std::scoped_lock _(mutex);
    if(!currentThreadTaskIndex) {
        return;
    }
    executedTasks.push_back(Info{
        .info = taskInfo,
        .postedTaskIndex = currentThreadTaskIndex
    });
}

void LogTaskTracker::OnTaskStart(const Task::MetaInfo& taskInfo) {
    std::scoped_lock _(mutex);
    std::optional<size_t> index = FindIndexByID(taskInfo.id);

    if(index) {
        Info& info = executedTasks[*index];
        info.startTime = TimePoint::Now();
        currentThreadTaskIndex = *index;
    } else {
        // TODO: Untracked task found
        executedTasks.push_back(Info{
            .info = taskInfo,
            .executedThread = Thread::GetCurrentThreadName()
        });
        currentThreadTaskIndex = executedTasks.size() - 1;
    }
}

void LogTaskTracker::OnTaskFinish(const Task::MetaInfo& taskInfo) {
    std::scoped_lock _(mutex);
    Info* entry = FindByID(taskInfo.id);
    DASSERT(entry);
    entry->finishTime = TimePoint::Now();
    currentThreadTaskIndex = {};
}

LogTaskTracker::Info* LogTaskTracker::FindByID(TaskID id) {
    std::optional<size_t> index = FindIndexByID(id);
    return index ? &executedTasks[*index] : nullptr;
}

std::optional<size_t> LogTaskTracker::FindIndexByID(TaskID id) {
    auto it = std::ranges::find_if(
        executedTasks,
        [&](const Info& info) {
            return info.info.id == id;
        }
    );
    if(it != executedTasks.end()) {
        size_t index = std::distance(it, executedTasks.begin());
        return {index};
    }
    return {};
}