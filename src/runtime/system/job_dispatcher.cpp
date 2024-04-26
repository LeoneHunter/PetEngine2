#include "job_dispatcher.h"

#include <format>
#include <chrono>
#include <concepts>

#include "runtime/core/concurrency.h"
#include "runtime/core/util.h"

#include "runtime/platform/windows_platform.h"

#define JOBSYSTEM_NAMESPACE_BEGIN namespace JobSystem {
#define JOBSYSTEM_NAMESPACE_END }

static constexpr u32 k_ThreadNum = 8;
static constexpr u32 k_FibersNum = 16;

static std::chrono::high_resolution_clock::time_point ExecutionStartTimepoint{std::chrono::high_resolution_clock::now()};

// Global job system data shared between all fibers
static JobSystem::Dispatcher* g_Context = nullptr;

constexpr static u32 NOT_AN_INDEX = std::numeric_limits<u32>::max();

using hFiber = void*;

struct Mutex {
	u64					mutexID_;
	// List of workers blocked, waiting for this mutex
	std::list<hFiber>	blockedWorkers_;
};

template<typename ListType>
class Iterator {
public:

	using value_type = typename ListType::value_type;
	using node_type = typename ListType::node_type;

	constexpr Iterator()
		: node_() {}

	constexpr Iterator(node_type* inHead)
		: node_(inHead) {}

	NODISCARD constexpr value_type& operator*() const {
		return node_->Value;
	}

	NODISCARD constexpr value_type* operator->() const {
		return &node_->Value;
	}

	constexpr Iterator& operator++() {
		node_ = node_->Next;
		return *this;
	}

	NODISCARD constexpr bool operator==(const Iterator& right) const {
		return node_ == right.node_;
	}

	node_type* node_ = nullptr;
};

template < class T >
concept trivial = std::is_trivial_v<T>;

template<trivial T>
class SynchronizedQueue {
public:

	NONCOPYABLE(SynchronizedQueue);

	struct Node {
		T		Value = T();
		Node*	Next = nullptr;
	};

	using value_type = T;
	using node_type = Node;
	using iterator = Iterator<SynchronizedQueue>;

public:

	constexpr SynchronizedQueue()
		: head_(nullptr)
		, tail_(nullptr) {}

	~SynchronizedQueue() {
		for(auto it = begin(); it != end(); ++it) {
			delete it.node_;
		}
	}

	void Push(T inVal) {
		Spinlock::ScopedLock lock(lock_);
		if(!head_) {
			head_ = new Node{inVal, nullptr};
			tail_ = head_;
		} else {
			auto* oldTail = tail_;
			tail_ = new Node{inVal, nullptr};
			oldTail->Next = tail_;
		}
	}

	NODISCARD T PopOrDefault(T inDefault) {
		Spinlock::ScopedLock lock(lock_);
		if(head_ == nullptr) {
			return inDefault;
		} else {
			auto* oldHead = head_;
			head_ = oldHead->Next;
			auto ret = oldHead->Value;

			if(oldHead == tail_) {
				tail_ = nullptr;
			}

			delete oldHead;
			return ret;
		}
	}

	NODISCARD constexpr bool Empty() const {
		Spinlock::ScopedLock lock(lock_);
		return head_ == nullptr;
	}

	iterator begin() const {
		return iterator{head_};
	}

	iterator end() const {
		return iterator{};
	}

private:
	mutable Spinlock	lock_;
	Node*				head_;
	Node*				tail_;
};


JOBSYSTEM_NAMESPACE_BEGIN

class Dispatcher;
struct ThreadContext;
struct Fiber;

class  JobGraph;
struct Job;
struct JobGroup;
class  Event;

void MasterFiberProc(std::string_view inThreadDebugName);
void WINAPI WorkerFiberProc(windows::LPVOID inParameter);

/*
* Object referenced by a job group and JobSystem::Fence created by the user
*/
struct FenceObject {

	constexpr FenceObject() = default;
	constexpr FenceObject(u64 inID): iD_(inID) {}
	constexpr explicit operator bool() const { return iD_ != 0; }

	u64		iD_ = 0;
	Fiber*	waitingWorkerFiber_ = nullptr;
};

struct Job {
	Job() = default;

	enum class Flags {
		None,
		WaitingOnMutex,
		WaitingOnCounter
	};

	std::string		debugName_;
	Flags			flags_ = Flags::None;
	CallableType	jobCallable_;
	hFiber			suspendedFiber_ = nullptr;
	JobGroup*		parentGroup_ = nullptr;
};

struct JobGroup {
	JobGroup() = default;

	std::list<Job>		jobs_;
	std::atomic<u64>	counter_;
	JobGroup*			nextGroup_ = nullptr;
	u64					fenceID_ = 0;

	EventRef			fenceEvent_;
};

enum class FiberSwitchCode {
	Initial,
	JobComplete,
	EventWait
};

struct ThreadContext {
	hFiber			MasterFiber = nullptr;
	Job*			Job = nullptr;
	FiberSwitchCode	SwitchCode = FiberSwitchCode::Initial;
	uint64_t		FenceMutexID = 0;
	Event*			Event = nullptr;

	void ClearSwitchData() {
		SwitchCode = FiberSwitchCode::Initial;
		FenceMutexID = 0;
		Event = nullptr;
	}
};

/*
* Shared data accessible to master and worker fibers
*/
struct Fiber {

	Fiber()
		: FiberHandle()
		, FiberIndex()
		, Job(nullptr)
		, ThreadContext(nullptr) {}

	void Init(hFiber inFiberHandle, u32 inFiberIndex) {
		FiberHandle = inFiberHandle;
		FiberIndex = inFiberIndex;
	}

	bool IsRunning() const {
		return Job != nullptr;
	}

	// Switch to this fiber
	// Should be called from master
	void SwitchTo(ThreadContext* inContext) {
		Assert(windows::GetCurrentFiber() != FiberHandle && "Cannot switch into itself");
		Assert(inContext && "Master fiber context not set");
		ThreadContext = inContext;

		// Synchronize master and our jobs
		if(!ThreadContext->Job) {
			Assert(Job);
			ThreadContext->Job = Job;

		} else if(!Job) {
			Assert(ThreadContext->Job);
			Job = ThreadContext->Job;
		}
		windows::SwitchToFiber(FiberHandle);
	}

	void WaitForEvent(Event& inEvent) {
		Assert(windows::GetCurrentFiber() == FiberHandle && "Could be called only from itself");

		ThreadContext->SwitchCode = FiberSwitchCode::EventWait;
		ThreadContext->Event = &inEvent;
		SwitchToMaster();
	}

	void Run() {

		while(true) {			
			Job = ThreadContext->Job;
			Assert(Job && "A fiber has been switched to without setting the job");
			Assert(ThreadContext && "Master fiber context not set");

			JobContext jobContext;

			auto& job = Job->jobCallable_;
			Assert(job && "The job callable is empty");

			job(jobContext);
			OnJobComplete();
		}
	}

	hFiber GetHandle() const {
		return FiberHandle;
	}

private:

	void SwitchToMaster() const {
		Assert(ThreadContext);
		windows::SwitchToFiber(ThreadContext->MasterFiber);
	}

	void OnJobComplete() {
		Job = nullptr;
		ThreadContext->SwitchCode = FiberSwitchCode::JobComplete;
		SwitchToMaster();
	}

private:

	hFiber			FiberHandle;
	u32				FiberIndex;
	// Job a fiber currently executing
	Job*			Job;
	ThreadContext*	ThreadContext;
};

/*
* Manages graphs of tasks (jobs)
*
* - Log per fiber
* - Fibers pre-created and put into wait list and pulled from the list when needed
* - Interdependent graphs (a job can depend on a job from another graph) (should be validated on creation (for cycles and stray nodes))
* - A leaf jobs list per graph
*
*	TODO
* - Output data (future)
* - Priorities
* - Debugging and profiling
* - Wait for custom event 
* - Wait for a resource to be loaded
*
*/
class Dispatcher {
public:

	Dispatcher(u32 inFibersNum) {
		g_Context = this;
		// Create fiber pool
		fibersPool_.assign(k_FibersNum, Fiber());

		for(u32 i = 0; i != k_FibersNum; ++i) {
			const auto fiberIndex = i;
			auto& fiber = fibersPool_[i];

			windows::LPVOID hWorkerFiber;
			hWorkerFiber = windows::CreateFiber(0, WorkerFiberProc, (windows::LPVOID)&fiber);
			Assert(hWorkerFiber != NULL && "Cannot create a fiber");

			fiber.Init(hWorkerFiber, fiberIndex);
			fibersFree_.Push(&fiber);
		}

		readyJobSemaphore_ = windows::CreateSemaphore(0, 1);
		Assert(readyJobSemaphore_ != NULL && "CreateSemaphore error");

		// Create threads
		auto WorkerThreadNum = k_ThreadNum ? k_ThreadNum : std::thread::hardware_concurrency();

		for(auto i = 0; i != WorkerThreadNum; ++i) {
			workerThreads_.emplace_back(MasterFiberProc, std::format("Worker {}", i));
		}
		LOGF(Verbose, "Dispatcher has created {} fibers on {} threads.", k_FibersNum, WorkerThreadNum);
	}

	~Dispatcher() {
		LOGF(Verbose, "Dispatcher destructor has been called.");
		exit_.store(true, std::memory_order::relaxed);
		AwakenAll();

		for(auto& thread : g_Context->workerThreads_) {
			thread.join();
		}

		for(u32 i = 0; i != k_FibersNum; ++i) {
			auto& fiber = fibersPool_[i];
			windows::DeleteFiber(fiber.GetHandle());
		}
	}

	// If called is a fiber returns it's state
	Fiber* GetCurrentFiber() {
		const auto currentFiber = windows::GetCurrentFiber();
		auto it = std::ranges::find_if(fibersPool_, [=](const Fiber& inFiber) { return inFiber.GetHandle() == currentFiber; });

		if(it != fibersPool_.end()) {
			return &*it;
		}
		return nullptr;
	}

	bool ShouldExit() {
		return exit_.load(std::memory_order::relaxed);
	}

	// Get fiber from free list
	Fiber* GetFreeFiber() {
		auto* freeFiber = fibersFree_.PopOrDefault(nullptr);
		Assert(freeFiber && "Out of free fibers!");
		return freeFiber;
	}

	// Return fiber to free list
	void ReleaseFiber(Fiber* inFiber) {
		Assert(inFiber);
		fibersFree_.Push(inFiber);
	}

	// Resumes suspended fiber
	void ResumeFiber(Fiber* inFiber) {		
#if DEBUG
		// TODO Assert that no other thread uses that fiber
#endif
		resumedFibers_.Push(inFiber);
		AwakenAll();
	}

	// Puts calling thread to sleep
	void WaitForJob() {
		// zero-second time-out interval
		windows::WaitForSingleObject(readyJobSemaphore_, 0L);           
	}

	void AwakenAll() {
		//// Notify threads that the job is ready
		windows::ReleaseSemaphore(
			readyJobSemaphore_,	// handle to semaphore
			1,						// increase count by one
			NULL);
	}

	Job* PollJob() {
		return readyJobListSync_.PopOrDefault(nullptr);
	}

	Fiber* PopUnlockedFiber() {
		return resumedFibers_.PopOrDefault(nullptr);
	}

	// Decrement group counter and manages graphs
	void OnJobComplete(Job* inJob) {
		Assert(inJob);

		auto* jobGroup = inJob->parentGroup_;
		Assert(jobGroup);

		const auto counter = jobGroup->counter_.fetch_sub(1, std::memory_order::relaxed) - 1;

		// The fiber called us has executed the last job in the group
		if(counter == 0) {
			auto* nextGroup = jobGroup->nextGroup_;

			if(nextGroup) {
				g_Context->EnqueueGroup(nextGroup);
			}

			if(jobGroup->fenceEvent_) {
				jobGroup->fenceEvent_.Signal();
			}
			delete jobGroup;
		}
	}

	// Enqueue job group for executing
	void EnqueueGroup(JobGroup* inGroup) {
		Assert(inGroup && !inGroup->jobs_.empty());

		for(auto& job : inGroup->jobs_) {
			{
				OPTICK_EVENT("Releasing a job");
				g_Context->readyJobListSync_.Push(&job);
				AwakenAll();
			}
		}
	}

	// Creates graph of jobs combined into groups for parallel execution
	void BuildGraph(const JobSystem::Builder& inBuilder) {
		Assert(!inBuilder.jobs_.empty() && "Submitted graph is empty!");

		auto jobIt = inBuilder.jobs_.begin();
		auto fenceIt = inBuilder.fences_.begin();

		JobGroup* outGroupPredecessor = nullptr;
		// First group in the graph
		JobGroup* outGroupHead = nullptr;

		while(jobIt != inBuilder.jobs_.end()) {
			const auto inGroupCounterID = jobIt->groupCounterID_;
			auto* newGroup = new JobGroup();
			auto& outGroup = *newGroup;
			auto& outJobs = outGroup.jobs_;

			for(; jobIt != inBuilder.jobs_.end() && jobIt->groupCounterID_ == inGroupCounterID; ++jobIt) {
				auto& outJob = outJobs.emplace_back();
				outJob.debugName_ = jobIt->debugName_;
				outJob.jobCallable_ = jobIt->jobCallable_;
				outJob.parentGroup_ = &outGroup;

				Assert(outJob.jobCallable_ && "Job callable is empty!");
			}
			outGroup.counter_.store(outJobs.size(), std::memory_order::relaxed);

			if(outGroupPredecessor) {
				outGroupPredecessor->nextGroup_ = &outGroup;
			} else {
				outGroupHead = newGroup;
			}
			outGroupPredecessor = &outGroup;

			if(fenceIt != inBuilder.fences_.end() && fenceIt->predecessorGroupCounterID_ == inGroupCounterID) {
				// User has destroyed the event, so ignore
				if(fenceIt->event_.GetRefCount() != 1) {
					outGroup.fenceEvent_ = fenceIt->event_;
				}
				++fenceIt;
			}
		}
		EnqueueGroup(outGroupHead);
	}

private:

	// Request exit of all threads
	std::atomic_bool					exit_;

	SynchronizedQueue<Job*>				readyJobListSync_;
	std::list<std::thread>				workerThreads_;

	// Pool of free fibers
	std::vector<Fiber>					fibersPool_;
	SynchronizedQueue<Fiber*>			fibersFree_;
	SynchronizedQueue<Fiber*>			resumedFibers_;

	// When there's no awailable work, thread go to sleep and awakened by this object
	windows::HANDLE						readyJobSemaphore_;
};

/*
* User space fiber synchronization event
* A fiber can wait for an event until it's signalled from another fiber
* Refcounted, referenced hold by notifier and notifee
*/
class Event {
public:

	Event() : waitFiberList_(), mb_Signalled(false) {}

	void Incref() {
		refCounter_.fetch_add(1, std::memory_order::relaxed);
	}

	void Decref() {
		const auto count = refCounter_.fetch_sub(1, std::memory_order::relaxed);

		if(count == 1) {
			Assert(waitFiberList_.empty());
			delete this;
		}
	}

	u32 GetRefCount() const {
		return refCounter_.load(std::memory_order::relaxed);
	}

	void Signal() {
		{
			Spinlock::ScopedLock lock(mutex_);

			if(mb_Signalled) {
				return;
			}
			mb_Signalled = true;
		}
		ResumeFibers();
	}

	bool IsSignalled() const {
		Spinlock::ScopedLock lock(mutex_);
		return mb_Signalled;
	}

	void ResumeFibers() {
		for(auto fiberit = waitFiberList_.begin(); fiberit != waitFiberList_.end(); ++fiberit) {
			g_Context->ResumeFiber(*fiberit);
		}
		waitFiberList_.clear();
	}

	bool PushWaitingFiber(Fiber* inFiber) {
		Spinlock::ScopedLock lock(mutex_);

		if(mb_Signalled) {
			return false;
		}
#if DEBUG 
		auto it = std::ranges::find(waitFiberList_, inFiber);
		Assert(it == waitFiberList_.end() && "This fiber is already waiting");
#endif
		waitFiberList_.push_back(inFiber);
		return true;
	}

private:
	mutable Spinlock	mutex_;
	std::list<Fiber*>	waitFiberList_;
	bool				mb_Signalled;

	std::atomic<u32>	refCounter_;
};

/*
* Algorithm:
*	#1 When awakened or just created
*		Check if have unblocked workers
*			Take one and resume
*		Check if have ready jobs
*			Take a new worker from the list and do a job
*		If no jobs are awailable, go to sleep (wait for cond.var. or atomic_wait)
*	#2 When worker switched back, check the switch code
*		If worker wants mutex
*			Check if the mutex is already taken
*				If taken:
*					IF no other jobs are available, spinlock and wait
*					ELIF have other jobs:
*						Put worker to suspended list
*						Store worker handle with the mutex
*						Take new fiber from the free list and take a new job
*		If wants to unlock:
*			Unlock the mutex
*			IF has workers waiting for it
*				Put one waiting worker into unblocked workers list
*			Continue with current worker
*		If wants to add new graph
*			Validate the graph for cycles and stray nodes
*			Parse graph, i.e. find leaf nodes without dependencies
*			Add leaf jobs to awailable list
*			If some threads are sleeping, awake
*			Switch back to the worker
*		If wants to wait for some jobs
*			Bind these jobs with pointers
*			Increase the dependencies counter on the waiting job
*			If called from the middle of execution
*				Put fiber to suspended list
*		If finished the job, process its successors (could be multiple)
*			#3 ForEach Successor node of the finished node
*				Decrease the predecessor counter
*				If zero,
*					Check if fiber executing the node is suspended and waiting for a dependency
*						If No more successors to process: remove and resume the fiber
*						Else Put the job to awailable list, clear flags and notify a sleeping thread
*					If not suspended, new job
*						If No more successors to process: take the job
*						Else Put the job to awailable list and goto #3
*			Go to #1
*/
void MasterFiberProc(std::string_view inThreadDebugName) {
	const auto threadDebugName = std::string(inThreadDebugName);

	windows::SetThreadDescription(windows::GetCurrentThread(), util::ToWideString(threadDebugName).c_str());
	OPTICK_THREAD(inThreadDebugName.data());
	
	ThreadContext context;
	context.MasterFiber = windows::ConvertThreadToFiber(nullptr);
	Assert(context.MasterFiber != NULL && "Cannot create master fiber");
	LOGF(Verbose, "Thread '{}' has started.", threadDebugName);	

	Fiber* fiber = nullptr;

	for(;;) {		
		Fiber* unlockedFiber = nullptr;

		// Check if have jobs otherwise sleep
		for(;;) {
			context.Job = g_Context->PollJob();
			unlockedFiber = g_Context->PopUnlockedFiber();

			if(!context.Job && !unlockedFiber) {

				// Exit when all jobs are finished
				if(g_Context->ShouldExit()) {
					return;
				}
				g_Context->WaitForJob();

			} else {
				break;
			}
		}

		// If we took unlocked fiber, hand back old one
		if(unlockedFiber) {

			if(fiber) {
				g_Context->ReleaseFiber(fiber);
			}
			fiber = unlockedFiber;
		}
				
		// If first time executins, take one new fiber
		if(!fiber) {
			fiber = g_Context->GetFreeFiber();
			Assert(fiber);
			//LOGF(Verbose, "Thread '{}' took a fiber '{}'.", inThreadDebugName, fiber->FiberIndex);
		}

		// Process fiber return codes
		for(;;) {
			fiber->SwitchTo(&context);

			if(context.SwitchCode == FiberSwitchCode::JobComplete) {
				LOGF(Verbose, "Thread '{}' has completed a job '{}'.", inThreadDebugName, context.Job->debugName_);
				g_Context->OnJobComplete(context.Job);
				context.Job = nullptr;
				break;

			} else if(context.SwitchCode == FiberSwitchCode::EventWait) {
				Assert(context.Event != nullptr);
				const auto result = context.Event->PushWaitingFiber(fiber);

				// If result == false, event has just been signalled, so ignore
				if(result) {
					fiber = nullptr;
					break;
				}

			} else {
				Assert(false && "Unknown return code");
			}
			context.ClearSwitchData();
		}
	}
	LOGF(Verbose, "Thread '{}' exits.", inThreadDebugName);
}

// Worker Fiber
void WINAPI WorkerFiberProc(windows::LPVOID inParameter) {
	// Context containts info about a thread we run on and a pointer to the master fiber that took us from free list
	// A worker fiber can run on any thread so we need to update this always
	auto* thisFiber = (Fiber*)inParameter;
	Assert(thisFiber);
	thisFiber->Run();
}

void JobSystem::Init() {
	if(!g_Context) {
		g_Context = new Dispatcher(k_FibersNum);
	}	
}

void JobSystem::Shutdown() {
	LOGF(Verbose, "job system destructor has been called.");
	if(g_Context) {
		delete g_Context;
		g_Context = nullptr;
	}
}

void JobSystem::Submit(const Builder& inJobBuilder) {
	// Check not empty
	Assert(g_Context);
	g_Context->BuildGraph(inJobBuilder);
}

void ThisFiber::WaitForEvent(EventRef inEventRef) {
	Assert(inEventRef && "Reference is null");
	if(inEventRef.IsSignalled()) {
		return;
	}
	g_Context->GetCurrentFiber()->WaitForEvent(*inEventRef.event_);
}

EventRef JobSystem::CreateEvent() {
	// Will be deleted by EventRef RAII object
	auto eventObject = new Event();
	return EventRef(eventObject);
}

EventRef Builder::PushFence() {
	Assert(!jobs_.empty());
	auto& fence = fences_.emplace_back();
	++groupCounter_;
	auto fenceEvent = new Event();
	fence.event_ = fenceEvent;
	return EventRef(fenceEvent);
}

EventRef::EventRef(Event* inEvent)
	: event_(inEvent)
{
	if(event_) {
		event_->Incref();
	}
}

EventRef::~EventRef() {
	if(!event_) return;
	event_->Decref();	
}

void EventRef::Signal() const {
	Assert(event_ && "Reference is null");
	event_->Signal();
}

EventRef::EventRef(const EventRef& other)
	: event_(other.event_) {
	if(!event_) return;
	event_->Incref();
}

EventRef& EventRef::operator= (const EventRef& right) {
	if(!right.event_) return *this;
	if(event_) event_->Decref();
	event_ = right.event_;
	event_->Incref();
	return *this;
}

bool EventRef::IsSignalled() const {
	Assert(event_ && "Reference is null");
	return event_->IsSignalled();
}

u32 EventRef::GetRefCount() const {
	Assert(event_ && "Reference is null");
	return event_->GetRefCount();
}

void EventRef::Reset() {
	if(event_) event_->Decref();
	event_ = nullptr;
}

JOBSYSTEM_NAMESPACE_END