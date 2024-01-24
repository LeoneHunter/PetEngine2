#pragma once

#include <mutex>
#include <thread>
#include <format>

#include "Defines.h"
#include "Runtime/Platform/WindowsPlatform.h"

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
			Windows::Pause();
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