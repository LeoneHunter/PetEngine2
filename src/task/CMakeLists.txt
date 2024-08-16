
module(
    NAME
        task
    HDRS
        future.h
        task_executor.h
        task_source.h
        task_tracker.h
        task.h
    SRCS
        task_executor.cpp
        task_source.cpp
        task_tracker.cpp
    DEPS
        base
)

test(
    NAME
        task_test
    SRCS
        future_test.cpp
        task_test.cpp
    DEPS
        task
)