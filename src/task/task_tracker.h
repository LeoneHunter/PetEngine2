#pragma once
#include "task.h"
#include "base/threading.h"

// Traces and logs tasks
class TaskTracker {
public:
    virtual ~TaskTracker() = default;
    virtual void OnTaskPost(const Task::MetaInfo& info) = 0;
    virtual void OnTaskStart(const Task::MetaInfo& info) = 0;
    virtual void OnTaskFinish(const Task::MetaInfo& info) = 0;
};

// Tracks all executed and posted tasks
class LogTaskTracker: public TaskTracker {
public:

    struct Info {
		Task::MetaInfo        info;
		TimePoint             startTime;
		TimePoint             finishTime;
		std::string           executedThread;
		std::optional<size_t> postedTaskIndex;
	};

    void OnTaskPost(const Task::MetaInfo& taskInfo) override;
    void OnTaskStart(const Task::MetaInfo& taskInfo) override;
    void OnTaskFinish(const Task::MetaInfo& taskInfo) override;

    Info* FindByID(TaskID id);
    std::optional<size_t> FindIndexByID(TaskID id);

    auto begin() const { return executedTasks.begin(); }
    auto end() const { return executedTasks.end(); }

public:
    std::mutex        mutex;
    std::vector<Info> executedTasks;
};