#include "future.h"
#include "thirdparty/doctest/doctest.h"

#include <thread>

TEST_CASE("[Future] Single thread, GetValue()") {
    {
        Promise<void> promise;
        Future<void> future = promise.GetFuture();
        CHECK(!future.IsReady());

        promise.Resolve();
        CHECK(future.IsReady());
    }
    {
        Promise<int> promise;
        Future<int> future = promise.GetFuture();
        CHECK(!future.IsReady());

        promise.Resolve(42);
        CHECK(future.IsReady());
        CHECK_EQ(future.GetValue(), 42);
    }
    {
        Promise<std::string> promise;
        Future<std::string> future = promise.GetFuture();
        promise.Resolve("42");
        CHECK_EQ(future.GetValue(), std::string("42"));
    }
    {
        using move_only = std::unique_ptr<int>;
        Promise<move_only> promise;
        Future<move_only> future = promise.GetFuture();

        auto val = move_only(new int(42));
        promise.Resolve(std::move(val));
        CHECK_EQ(*future.GetValue(), 42);
    }
    {
        Promise<void> promise;
        Future<void> future = promise.GetFuture();
        bool called = false;
        
        future.Then([&]() {
            called = true;
        });
        promise.Resolve();
        CHECK(called);
    }
}

TEST_CASE("[Future] Two threads, GetValue()") {
    Promise<int> promise;
    Future<int> future = promise.GetFuture();
    CHECK(!future.IsReady());

    auto thread = std::thread([promise = std::move(promise)]() mutable {
        promise.Resolve(42);
    });
    CHECK(promise.Empty());

    // Wait until the thread is started and check for synchronization
    while(!future.IsReady()) {
        std::this_thread::yield();
    }
    CHECK_EQ(future.GetValue(), 42);
    thread.join();
}

TEST_CASE("[Future] Two threads, Callback, Then() -> Resolve()") {
    using move_only = std::unique_ptr<int>;
    Promise<move_only> promise;
    Future<move_only> future = promise.GetFuture();

    future.Then([mo = move_only()](move_only result) mutable {
        CHECK_EQ(*result, 42);
    });

    auto thread = std::thread([promise = std::move(promise)]() mutable {
        promise.Resolve(std::make_unique<int>(42));
    });
    thread.join();
    CHECK(future.IsFinished());
}

TEST_CASE("[Future] Two threads, Callback, Resolve() -> Then()") {
    using move_only = std::unique_ptr<int>;
    Promise<move_only> promise;
    Future<move_only> future = promise.GetFuture();

    auto thread = std::thread([promise = std::move(promise)]() mutable {
        promise.Resolve(std::make_unique<int>(42));
    });
    thread.join();
    CHECK(future.IsReady());

    future.Then([mo = move_only()](move_only result) mutable {
        CHECK_EQ(*result, 42);
    });
    CHECK(future.IsFinished());
}