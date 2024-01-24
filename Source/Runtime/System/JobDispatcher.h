#pragma once

#include <iostream>
#include <chrono>
#include <vector>
#include <functional>
#include <list>
#include <thread>

#include "Runtime/Core/BaseTypes.h"
#include "Runtime/Core/Defines.h"
#include "Runtime/Core/ErrorMacros.h"
#include "Runtime/Core/Math.h"

#include "ThirdParty/Optik/include/optick.config.h"
#include "ThirdParty/Optik/include/optick.h"

/*
* 
* 
* TODO:
*	- Add internable strings
*	- Add backing storage with pool allocator for jobs
*/
namespace JobSystem {

	struct ThreadContext;
	class JobGraph;
	class Dispatcher;
	class Event;
	
	struct JobContext;
	using CallableType = std::function<void(JobSystem::JobContext&)>;

	class Builder;
	void Submit(const Builder& inJobBuilder);

	// Returned to users
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

		operator bool() const { return m_Event != nullptr; }

	public:
		Event* m_Event = nullptr;
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
		void PushBack(std::string_view inJobName, const CallableType& inJobCallable) {
			Assert(inJobCallable);
			Emplace(inJobName, inJobCallable, ++m_GroupCounter);
		}

		/*
		* Add a job parallel to previously added
		*/
		void PushAsync(std::string_view inJobName, const CallableType& inJobCallable) {
			Assert(inJobCallable);			
			Emplace(inJobName, inJobCallable, m_GroupCounter);
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
			JobSystem::Submit(*this);
		}

	public:

		u64	GetGroupNum() const { return m_GroupCounter; }

	private:

		struct Job {
			Job() = default;

			std::string		m_DebugName;
			CallableType	m_JobCallable;
			// Counter associated with this job
			// Multiple jobs could be associated with a single counter
			// Acts as a fence, successor jobs cannot be executed until it reaches 0
			// Here we store id of a counter shared across parallel jobs
			u64				m_GroupCounterID = 0;
		};

		/*
		* Manual syncronization point
		* A job can wait until this fence is reached
		*/
		struct FenceInternal {
			FenceInternal() = default;

			u64			m_PredecessorGroupCounterID = 0;
			// Event referenced by a user
			EventRef	m_Event;
		};

		void Emplace(std::string_view inJobName, const CallableType& inJobCallable, u32 inCounter) {
			auto& newJobNode = m_Jobs.emplace_back();
			newJobNode.m_DebugName = inJobName;
			newJobNode.m_GroupCounterID = inCounter;
			newJobNode.m_JobCallable = inJobCallable;
		}

	private:
		// ID of a group of jobs that could be executed concurrently
		u32							m_GroupCounter = 0;
		// List of jobs grouped by counters
		std::list<Job>				m_Jobs;
		std::list<FenceInternal>	m_Fences;
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