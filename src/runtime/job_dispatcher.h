#pragma once

#include <iostream>
#include <chrono>
#include <vector>
#include <functional>
#include <list>
#include <thread>

#include "types.h"
#include "error.h"
#include "math_util.h"

#include "thirdparty/optik/include/optick.config.h"
#include "thirdparty/optik/include/optick.h"

/*
* TODO:
*	- Add internable strings
*	- Add backing storage with pool allocator for jobs
*/
namespace fiber {

	struct ThreadContext;
	class JobGraph;
	class Dispatcher;
	class Event;
	
	struct JobContext;
	using CallableType = std::function<void(fiber::JobContext&)>;

	class Builder;
	void Submit(const Builder& inJobBuilder);

	/*
	* RAII object that points to a fiber event object
	* An event object is created by the fiber and an EventRef is 
	* returned to caller. After event is signalled the fiber destroys it's EventRef instance
	* And the caller can quiery the state of the event and safely destroy the EventRef object
	* 
	* TODO could be replaced with std::shared_ptr
	*/
	struct EventRef {

		EventRef() = default;
		EventRef(Event* inEvent);
		~EventRef();

		EventRef(const EventRef& other);
		EventRef& operator= (const EventRef& right);

		// Signals all fibers waiting
		void Signal() const;
		bool IsSignalled() const;
		u32  GetRefCount() const;

		// Destroys the event object if it has no other references
		// and sets this to nullptr
		void Reset();

		operator bool() const { return event_ != nullptr; }

	public:
		Event* event_ = nullptr;
	};

	/*
	* Builds job graph which can be submitted to the job system
	* Actually now it'a branch builder
	*/
	class Builder {
	public:
		friend class Dispatcher;

	public:

		/*
		* Add a job consequent to previously added
		*/
		void PushBack(const CallableType& inJobCallable, std::string_view inJobName = "") {
			Assert(inJobCallable);
			Emplace(inJobName, inJobCallable, ++groupCounter_);
		}

		/*
		* Add a job parallel to previously added
		*/
		void PushAsync(const CallableType& inJobCallable, std::string_view inJobName = "") {
			Assert(inJobCallable);			
			Emplace(inJobName, inJobCallable, groupCounter_);
		}

		/*
		* Makes all later added jobs to depend on already added
		* - Check if has predecessor jobs
		*/
		EventRef PushFence();

		/*
		* Kick and wait for completion
		*/
		void KickAndWait() {}

		/*
		* Kick jobs
		*/
		void Kick() {			
			fiber::Submit(*this);
		}

	public:

		u64	GetGroupNum() const { return groupCounter_; }

	private:

		struct Job {
			Job() = default;

			std::string		debugName_;
			CallableType	jobCallable_;
			// Counter associated with this job
			// Multiple jobs could be associated with a single counter
			// Acts as a fence, successor jobs cannot be executed until it reaches 0
			// Here we store id of a counter shared across parallel jobs
			u64				groupCounterID_ = 0;
		};

		/*
		* Manual syncronization point
		* A job can wait until this fence is reached
		*/
		struct FenceInternal {
			FenceInternal() = default;

			u64			predecessorGroupCounterID_ = 0;
			// Event referenced by a user
			EventRef	event_;
		};

		void Emplace(std::string_view inJobName, const CallableType& inJobCallable, u32 inCounter) {
			auto& newJobNode = jobs_.emplace_back();
			newJobNode.debugName_ = inJobName;
			newJobNode.groupCounterID_ = inCounter;
			newJobNode.jobCallable_ = inJobCallable;
		}

	private:
		// ID of a group of jobs that could be executed concurrently
		u32							groupCounter_ = 0;
		// List of jobs grouped by counters
		std::list<Job>				jobs_;
		std::list<FenceInternal>	fences_;
	};

	struct JobContext {
		//void Lock(uint64_t inMutexID);
		//void Unlock(uint64_t inMutexID);
		ThreadContext* ThreadCtx = nullptr;
	};

	// Creates threads, fibers and context
	void Init();

	void Shutdown();

	void Submit(const Builder& inJobBuilder);

	// Creates an Event object
	// A fiber can wait for an Event to be signalled
	// The fiber is suspended in saved state while waiting
	// The Event object itself is ref counted by RAII
	// @return EventRef - RAII object, stores pointer to the Event and decrements it's ref
	// count when destroyed
	EventRef CreateEvent();

	namespace ThisFiber {

		void WaitForEvent(EventRef inEventRef);

		//void WaitFor(Duration inDurationMs);
		//void Yield();
	}
}