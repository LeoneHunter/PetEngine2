#pragma once
#include <semaphore>
#include <thread>
#include <mutex>

#include "common.h"
#include "util.h"
#include "win_minimal.h"

template<>
struct std::formatter<std::thread::id, char> {
	bool quoted = false;

	template<class ParseContext>
	constexpr ParseContext::iterator parse(ParseContext& ctx) {
		return ctx.begin();
	}

	template<class FmtContext>
	FmtContext::iterator format(std::thread::id s, FmtContext& ctx) const {
		std::ostringstream out;
		out << s;
		return std::ranges::copy(std::move(out).str(), ctx.out()).out;
	}
};

// From Boost
class Spinlock {
public:

	std::atomic_flag v_;

public:

	bool try_lock() {
		return !v_.test_and_set(std::memory_order_acquire);
	}

	void lock() {
		while(!try_lock()) {
			windows::Pause();
		}
	}

	void unlock() {
		v_.clear(std::memory_order_release);
	}

public:

	class ScopedLock {
	private:

		Spinlock& sp_;

		ScopedLock(ScopedLock const&) = delete;
		ScopedLock& operator=(ScopedLock const&) = delete;

	public:

		explicit ScopedLock(Spinlock& sp)
			: sp_(sp) {
			sp.lock();
		}

		~ScopedLock() {
			sp_.unlock();
		}
	};
};

using ThreadID = windows::DWORD;

// Basic thread wrapper around std::thread
// Could be created by the user or inside a thread pool
// Own by the user or thread pool
class Thread {
public:

	static void SetCurrentThreadName(const std::string& name);
    static const std::string& GetCurrentThreadName();

    static ThreadID GetCurrentThreadID();

public:

    class Delegate {
    public:
        virtual ~Delegate() = default;
        virtual void WorkerMain(bool canSleep) = 0;
    };

public:

    void Start() {
        Assert(!thread_);
        Assert(delegate_);
        thread_ = std::make_unique<std::thread>([this]{
            SetCurrentThreadName(name_);
            threadStartedSemaphore_.release();

            constexpr bool canSleep = true;
            delegate_->WorkerMain(canSleep);
        });
    }

    // For testing
    void WaitUntilStarted() {
        Assert(thread_);
        threadStartedSemaphore_.acquire();
    }

    void Join() {
        Assert(thread_);
        Assert(thread_->joinable());
        thread_->join();
    }

public:
	// TODO: Decide delegate ownership
    Thread(Delegate* delegate, const std::string& name = "") {
        Assert(delegate);
        delegate_ = delegate;
        name_ = name;
    }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // Because of semaphore and lambda inside Start()
    Thread(Thread&) = delete;
    Thread& operator=(Thread&) = delete;

private:
	Delegate*                    delegate_ = {};
	std::string                  name_;
	std::binary_semaphore        threadStartedSemaphore_{0};
	std::unique_ptr<std::thread> thread_;
};


