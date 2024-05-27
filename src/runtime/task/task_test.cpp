#include "task.h"
#include "thirdparty/doctest/doctest.h"

#include "runtime/threading.h"
#include "task_tracker.h"
#include "task_executor.h"
#include "task_source.h"

namespace {

class DummyTracker: public TaskTracker {
public:
    void OnTaskPost(const Task::MetaInfo& taskInfo) override {}
    void OnTaskStart(const Task::MetaInfo& taskInfo) override {}
    void OnTaskFinish(const Task::MetaInfo& taskInfo) override {}
};

int data = 0;
void FreestandingFunc(int arg) {}

struct MyClass {
    void MyMember(int* out, int arg2) { *out = arg2; }
    void MyMemberConst(int arg) const {}
    int data = 42;
};

template<class T>
struct MoveOnly {
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&& rhs) { std::swap(data, rhs.data); }
    MoveOnly& operator=(MoveOnly&& rhs) { data = 0; std::swap(data, rhs.data); }

    MoveOnly(T data = {}): data(data) {}

    T data;
};

std::string kMainThreadName = "Main Thread";
std::string kWorkerThreadPrefix = "Worker Thread";

} // namespace

TEST_CASE("[Task] Binding") {
    int result = 0;
    // Captureless lambda
    {
        auto task = Task::Bind(
            std::source_location::current(),
            [](int& result) { result = 42; },
            result
        );
        std::move(task).Run();
        CHECK(!task);
    }
    // Free function
    {
        auto task2 = Task::Bind(
            std::source_location::current(),
            &FreestandingFunc,
            435
        );
        std::move(task2).Run();
        CHECK(!task2);
    }
    // Raw pointers
    {
        MyClass instance;
        const auto* p = &instance;
        int data = 0;

        auto task3 = Task::Bind(
            std::source_location::current(),
            &MyClass::MyMember,
            RawPointer(&instance),
            RawPointer(&data),
            435
        );
        std::move(task3).Run();
        CHECK_EQ(data, 435);

        auto task4 = Task::Bind(
            std::source_location::current(),
            &MyClass::MyMemberConst,
            RawPointer(p),
            435
        );
        std::move(task4).Run();
    }
    // Copying arguments
    {
        std::vector<int> vec = {1, 2, 3};

        // Vec should be copied into the Task tuple
        auto task = Task::Bind(
            std::source_location::current(),
            [](std::vector<int>& vec) { 
                vec[2] = 10;
            },
            vec
        );
        std::move(task).Run();
        // Value shouln't change
        CHECK_EQ(vec[2], 3);
    }
    // Move only types
    {
        auto move_only = std::make_unique<MyClass>();

        auto task = Task::Bind(
            std::source_location::current(),
            [](std::unique_ptr<MyClass>& move_only) { 
                move_only->data = 453;
            },
            std::move(move_only)
        );
        std::move(task).Run();
    }
}

TEST_CASE("[Task] Main thread with event loop") {
    auto tracker = std::make_shared<DummyTracker>();
    auto executor = std::make_unique<TaskExecutor>(tracker);
    auto eventLoop = std::make_shared<EventLoop>();
    executor->RegisterTaskSource(eventLoop);
    std::string result;

    eventLoop->PostTask(
        [&result]() {
            result.append("Hello ");

            EventLoop::GetForCurrentThread()->PostTask(
                [&result]() {
                    result.append("World!");
                }
            );
        }
    );
    executor->RunUntilIdle();
    CHECK_EQ(result, "Hello World!");

    executor.reset();
    eventLoop.reset();
}

TEST_CASE("[Task] Main thread with two event loops") {
    auto tracker = std::make_shared<DummyTracker>();
    auto executor = std::make_unique<TaskExecutor>(tracker);

    auto eventLoop1 = std::make_shared<EventLoop>();
    executor->RegisterTaskSource(eventLoop1);

    auto eventLoop2 = std::make_shared<EventLoop>();
    executor->RegisterTaskSource(eventLoop2);

    std::string result;

    eventLoop1->PostTask(
        [&]() {
            result.append("Hello ");

            eventLoop2->PostTask(
                [&]() {
                    result.append("World! ");

                    eventLoop1->PostTask(
                        [&]() {
                            result.append("I'm ");

                            eventLoop2->PostTask(
                                [&]() {
                                    result.append("a cat!");
                                }
                            );
                        }
                    );
                }
            );
        }
    );
    executor->RunUntilIdle();
    CHECK_EQ(result, "Hello World! I'm a cat!");

    executor.reset();
    eventLoop1.reset();
    eventLoop2.reset();
}

TEST_CASE("[Task] Main thread & Dedicated thread") {
    Thread::SetCurrentThreadName(kMainThreadName);

    auto tracker = std::make_shared<DummyTracker>();
    auto executor = std::make_unique<TaskExecutor>(tracker);

    auto eventLoop1 = std::make_shared<EventLoop>();
    executor->RegisterTaskSource(eventLoop1);

    auto worker = std::make_unique<DedicatedThread>(kWorkerThreadPrefix, tracker);
    auto eventLoop2 = worker->CreateEventLoop();
    worker->Start();

    SUBCASE("Two event loops passing tasks") {
        std::string result;

        eventLoop1->PostTask(
            [&]() {
                result.append("Hello ");
                CHECK_EQ(Thread::GetCurrentThreadName(), kMainThreadName);
                CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);

                eventLoop2->PostTask(
                    [&]() {
                        result.append("World! ");
                        CHECK_EQ(Thread::GetCurrentThreadName(), kWorkerThreadPrefix);
                        CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop2);

                        eventLoop1->PostTask(
                            [&]() {
                                result.append("I'm ");
                                CHECK_EQ(Thread::GetCurrentThreadName(), kMainThreadName);
                                CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);

                                eventLoop2->PostTask(
                                    [&]() {
                                        result.append("a cat!");
                                        CHECK_EQ(Thread::GetCurrentThreadName(), kWorkerThreadPrefix);

                                        // Stop main
                                        eventLoop1->PostTask(
                                            []() {
                                                CHECK_EQ(Thread::GetCurrentThreadName(), kMainThreadName);
                                                TaskExecutor::GetForCurrentThread()->Stop();
                                            }
                                        );
                                    }
                                );
                            }
                        );
                    }
                );
            }
        );
        executor->RunUntilStopped(/* canSleep */true);
        CHECK_EQ(result, "Hello World! I'm a cat!");
    }

    worker->Stop();
    worker->Join();

    executor.reset();
    eventLoop1.reset();
    eventLoop2.reset();
}

namespace {

struct ThreadPoolEnvironment {

    ThreadPoolEnvironment(int threadNum) {
        Thread::SetCurrentThreadName(kMainThreadName);

        tracker = std::make_shared<DummyTracker>();
        pool = std::make_unique<ThreadPool>(
            std::make_unique<TaskExecutor>(tracker),
            threadNum,
            kWorkerThreadPrefix);

        pool->Start();
        pool->WaitUntilStarted();

        eventLoop1 = std::make_shared<EventLoop>();
        pool->RegisterTaskSource(eventLoop1);

        eventLoop2 = std::make_shared<EventLoop>();
        pool->RegisterTaskSource(eventLoop2);
    }

    ~ThreadPoolEnvironment() {
        CHECK(eventLoop1->Empty());
        CHECK(eventLoop2->Empty());
        eventLoop1.reset();
        eventLoop2.reset();

        pool->Stop();
    }

    std::unique_ptr<ThreadPool>  pool;
    std::shared_ptr<TaskTracker> tracker;
    std::shared_ptr<EventLoop>   eventLoop1;
    std::shared_ptr<EventLoop>   eventLoop2;
};

struct ThreadPoolTest: public ThreadPoolEnvironment {

    constexpr static int kThreadNum = 3;

    ThreadPoolTest() 
        : ThreadPoolEnvironment(kThreadNum) 
    {}

    const static inline std::string kExpectedResult = "Hello World! I'm a cat!";
    std::string result;
};
std::binary_semaphore workDoneSemaphore{0};

} // namespace

TEST_CASE_FIXTURE(ThreadPoolTest, "[Task] Thread pool with two event loops") {
    eventLoop1->PostTask(
        [&]() {
            result.append("Hello ");
            CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);

            eventLoop2->PostTask(
                [&]() {
                    result.append("World! ");
                    CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop2);

                    eventLoop1->PostTask(
                        [&]() {
                            result.append("I'm ");
                            CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);

                            eventLoop2->PostTask(
                                [&]() {
                                    result.append("a cat!");
                                    workDoneSemaphore.release();
                                }
                            );
                        }
                    );
                }
            );
        }
    );
    workDoneSemaphore.acquire();
    CHECK_EQ(result, kExpectedResult);
}

TEST_CASE_FIXTURE(ThreadPoolTest, "[Task] EventLoop::PostTaskWithCallbackOn()") {
    eventLoop2->PostTaskWithCallbackOn(
        eventLoop1, 
        []() {
            return kExpectedResult;
        },
        [this](const std::string& result) {
            CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);
            this->result = result; 
            workDoneSemaphore.release();
        }
    );
    workDoneSemaphore.acquire();
    CHECK_EQ(result, kExpectedResult);
}

TEST_CASE_FIXTURE(ThreadPoolTest, "[Task] EventLoop::PostTaskWithCallbackOn() with MoveOnly") {
    eventLoop2->PostTaskWithCallbackOn(
        eventLoop1, 
        []() {
            MoveOnly<std::string> mo(kExpectedResult);
            return std::move(mo);
        },
        [this](MoveOnly<std::string> result) {
            CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);
            this->result = result.data; 
            workDoneSemaphore.release();
        }
    );
    workDoneSemaphore.acquire();
    CHECK_EQ(result, kExpectedResult);
}

TEST_CASE_FIXTURE(ThreadPoolTest, "[Task] EventLoop::PostTaskWithCallbackOn() with void") {
    eventLoop2->PostTaskWithCallbackOn(
        eventLoop1, 
        []() {
            std::this_thread::yield();
        },
        [this]() {
            CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);
            this->result = kExpectedResult; 
            workDoneSemaphore.release();
        }
    );
    workDoneSemaphore.acquire();
    CHECK_EQ(result, kExpectedResult);
}

TEST_CASE_FIXTURE(ThreadPoolTest, "[Task] EventLoop with future") {
    using FutureType = EventLoopFuture<MoveOnly<std::string>>;

    eventLoop1->PostTask([this]() {
        FutureType future = eventLoop2->PostTaskWithPromise([this]() {
            MoveOnly<std::string> mo(kExpectedResult);
            return std::move(mo);
        });
        future.ThenOnCurrent([this](MoveOnly<std::string> result) {
            CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);
            this->result = result.data; 
            workDoneSemaphore.release();
        });
    });
    workDoneSemaphore.acquire();
    CHECK_EQ(result, kExpectedResult);
}

TEST_CASE_FIXTURE(ThreadPoolTest, "[Task] EventLoop with void future") {
    using FutureType = EventLoopFuture<void>;

    eventLoop1->PostTask([this]() {
        FutureType future = eventLoop2->PostTaskWithPromise([this]() {
            std::this_thread::yield();
        });
        future.ThenOnCurrent([this]() {
            CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);
            this->result = kExpectedResult; 
            workDoneSemaphore.release();
        });
    });
    workDoneSemaphore.acquire();
    CHECK_EQ(result, kExpectedResult);
}

namespace {

struct EventLoopsWithCoroutine: public ThreadPoolTest {
    using FutureMO = EventLoopFuture<MoveOnly<std::string>>;

    // Runs on eventLoop1
    EventLoopFuture<void> Task() {
        FutureMO future = eventLoop2->PostTaskWithPromise([this]() {
            MoveOnly<std::string> mo(kExpectedResult);
            return std::move(mo);
        });
        // Suspend and resume on completion on eventLoop1
        MoveOnly<std::string> result = co_await std::move(future);

        CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);
        this->result = result.data; 
        workDoneSemaphore.release();
    }
    
    // Runs on eventLoop1
    EventLoopFuture<void> TaskVoid() {
        EventLoopFuture<void> future = eventLoop2->PostTaskWithPromise([this]() {
            std::this_thread::yield();
        });
        // Suspend and resume on completion on eventLoop1
        co_await std::move(future);

        CHECK_EQ(EventLoop::GetForCurrentThread(), eventLoop1);
        this->result = kExpectedResult; 
        workDoneSemaphore.release();
    }
};
    
} // namespace

TEST_CASE_FIXTURE(EventLoopsWithCoroutine, "[Task] EventLoop with coroutine") {
    using FutureType = EventLoopFuture<MoveOnly<std::string>>;

    eventLoop1->PostTask([this]() {
        (void)Task();
    });
    workDoneSemaphore.acquire();
    CHECK_EQ(result, kExpectedResult);
}

TEST_CASE_FIXTURE(EventLoopsWithCoroutine, "[Task] EventLoop with void coroutine") {
    using FutureType = EventLoopFuture<MoveOnly<std::string>>;

    eventLoop1->PostTask([this]() {
        (void)TaskVoid();
    });
    workDoneSemaphore.acquire();
    CHECK_EQ(result, kExpectedResult);
}
