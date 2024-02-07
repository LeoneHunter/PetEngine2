#include "JobDispatcher.h"

#include <format>
#include <chrono>
#include <concepts>

#include "Runtime/Core/Concurrency.h"
#include "Runtime/Core/Util.h"

#include "Runtime/Platform/WindowsPlatform.h"

#define JOBSYSTEM_NAMESPACE_BEGIN namespace JobSystem {
#define JOBSYSTEM_NAMESPACE_END }

static constexpr u32 k_ThreadNum = 8;
static constexpr u32 k_FibersNum = 16;

static std::chrono::high_resolution_clock::time_point ExecutionStartTimepoint{std::chrono::high_resolution_clock::now()};

// Global job system data shared between all fibers
static JobSystem::Dispatcher* g_Context = nullptr;

constexpr static u32 NOT_AN_INDEX = std::numeric_limits<u32>::max();
 
//#define LOGF(fmt, ...) \
//	std::cerr << std::format("[{}]: {}\n", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - ExecutionStartTimepoint)\
//	, std::format(fmt, __VA_ARGS__));

#define LOGF(fmt, ...)

using hFiber = void*;

struct Mutex {
	u64					m_MutexID;
	// List of workers blocked, waiting for this mutex
	std::list<hFiber>	m_BlockedWorkers;
};

template<typename ListType>
class Iterator {
public:

	using value_type = typename ListType::value_type;
	using node_type = typename ListType::node_type;

	constexpr Iterator()
		: m_Node() {}

	constexpr Iterator(node_type* inHead)
		: m_Node(inHead) {}

	NODISCARD constexpr value_type& operator*() const {
		return m_Node->Value;
	}

	NODISCARD constexpr value_type* operator->() const {
		return &m_Node->Value;
	}

	constexpr Iterator& operator++() {
		m_Node = m_Node->Next;
		return *this;
	}

	NODISCARD constexpr bool operator==(const Iterator& right) const {
		return m_Node == right.m_Node;
	}

	node_type* m_Node = nullptr;
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
		: m_Head(nullptr)
		, m_Tail(nullptr) {}

	~SynchronizedQueue() {
		for(auto it = begin(); it != end(); ++it) {
			delete it.m_Node;
		}
	}

	void Push(T inVal) {
		Spinlock::ScopedLock lock(m_Lock);
		if(!m_Head) {
			m_Head = new Node{inVal, nullptr};
			m_Tail = m_Head;
		} else {
			auto* oldTail = m_Tail;
			m_Tail = new Node{inVal, nullptr};
			oldTail->Next = m_Tail;
		}
	}

	NODISCARD T PopOrDefault(T inDefault) {
		Spinlock::ScopedLock lock(m_Lock);
		if(m_Head == nullptr) {
			return inDefault;
		} else {
			auto* oldHead = m_Head;
			m_Head = oldHead->Next;
			auto ret = oldHead->Value;

			if(oldHead == m_Tail) {
				m_Tail = nullptr;
			}

			delete oldHead;
			return ret;
		}
	}

	NODISCARD constexpr bool Empty() const {
		Spinlock::ScopedLock lock(m_Lock);
		return m_Head == nullptr;
	}

	iterator begin() const {
		return iterator{m_Head};
	}

	iterator end() const {
		return iterator{};
	}

private:
	mutable Spinlock	m_Lock;
	Node*				m_Head;
	Node*				m_Tail;
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
void WINAPI WorkerFiberProc(Windows::LPVOID inParameter);

/*
* Object referenced by a job group and JobSystem::Fence created by the user
*/
struct FenceObject {

	constexpr FenceObject() = default;
	constexpr FenceObject(u64 inID): m_ID(inID) {}
	constexpr explicit operator bool() const { return m_ID != 0; }

	u64		m_ID = 0;
	Fiber*	m_WaitingWorkerFiber = nullptr;
};

struct Job {
	Job() = default;

	enum class Flags {
		None,
		WaitingOnMutex,
		WaitingOnCounter
	};

	std::string		m_DebugName;
	Flags			m_Flags = Flags::None;
	CallableType	m_JobCallable;
	hFiber			m_SuspendedFiber = nullptr;
	JobGroup*		m_ParentGroup = nullptr;
};

struct JobGroup {
	JobGroup() = default;

	std::list<Job>		m_Jobs;
	std::atomic<u64>	m_Counter;
	JobGroup*			m_NextGroup = nullptr;
	u64					m_FenceID = 0;

	EventRef			m_FenceEvent;
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
		Assert(Windows::GetCurrentFiber() != FiberHandle && "Cannot switch into itself");
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
		Windows::SwitchToFiber(FiberHandle);
	}

	void WaitForEvent(Event& inEvent) {
		Assert(Windows::GetCurrentFiber() == FiberHandle && "Could be called only from itself");

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

			auto& job = Job->m_JobCallable;
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
		Windows::SwitchToFiber(ThreadContext->MasterFiber);
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
		m_FibersPool.assign(k_FibersNum, Fiber());

		for(u32 i = 0; i != k_FibersNum; ++i) {
			const auto fiberIndex = i;
			auto& fiber = m_FibersPool[i];

			Windows::LPVOID hWorkerFiber;
			hWorkerFiber = Windows::CreateFiber(0, WorkerFiberProc, (Windows::LPVOID)&fiber);
			Assert(hWorkerFiber != NULL && "Cannot create a fiber");

			fiber.Init(hWorkerFiber, fiberIndex);
			m_FibersFree.Push(&fiber);
		}

		m_ReadyJobSemaphore = Windows::CreateSemaphore(0, 1);
		Assert(m_ReadyJobSemaphore != NULL && "CreateSemaphore error");

		// Create threads
		auto WorkerThreadNum = k_ThreadNum ? k_ThreadNum : std::thread::hardware_concurrency();

		for(auto i = 0; i != WorkerThreadNum; ++i) {
			m_WorkerThreads.emplace_back(MasterFiberProc, std::format("Worker {}", i));
		}
		LOGF("Dispatcher has created {} fibers on {} threads.", k_FibersNum, WorkerThreadNum);
	}

	~Dispatcher() {
		LOGF("Dispatcher destructor has been called.");
		m_Exit.store(true, std::memory_order::relaxed);
		AwakenAll();

		for(auto& thread : g_Context->m_WorkerThreads) {
			thread.join();
		}

		for(u32 i = 0; i != k_FibersNum; ++i) {
			auto& fiber = m_FibersPool[i];
			Windows::DeleteFiber(fiber.GetHandle());
		}
	}

	// If called is a fiber returns it's state
	Fiber* GetCurrentFiber() {
		const auto currentFiber = Windows::GetCurrentFiber();
		auto it = std::ranges::find_if(m_FibersPool, [=](const Fiber& inFiber) { return inFiber.GetHandle() == currentFiber; });

		if(it != m_FibersPool.end()) {
			return &*it;
		}
		return nullptr;
	}

	bool ShouldExit() {
		return m_Exit.load(std::memory_order::relaxed);
	}

	// Get fiber from free list
	Fiber* GetFreeFiber() {
		auto* freeFiber = m_FibersFree.PopOrDefault(nullptr);
		Assert(freeFiber && "Out of free fibers!");
		return freeFiber;
	}

	// Return fiber to free list
	void ReleaseFiber(Fiber* inFiber) {
		Assert(inFiber);
		m_FibersFree.Push(inFiber);
	}

	// Resumes suspended fiber
	void ResumeFiber(Fiber* inFiber) {		
#if DEBUG
		// TODO Assert that no other thread uses that fiber
#endif
		m_ResumedFibers.Push(inFiber);
		AwakenAll();
	}

	// Puts calling thread to sleep
	void WaitForJob() {
		// zero-second time-out interval
		Windows::WaitForSingleObject(m_ReadyJobSemaphore, 0L);           
	}

	void AwakenAll() {
		//// Notify threads that the job is ready
		Windows::ReleaseSemaphore(
			m_ReadyJobSemaphore,	// handle to semaphore
			1,						// increase count by one
			NULL);
	}

	Job* PollJob() {
		return m_ReadyJobListSync.PopOrDefault(nullptr);
	}

	Fiber* PopUnlockedFiber() {
		return m_ResumedFibers.PopOrDefault(nullptr);
	}

	// Decrement group counter and manages graphs
	void OnJobComplete(Job* inJob) {
		Assert(inJob);

		auto* jobGroup = inJob->m_ParentGroup;
		Assert(jobGroup);

		const auto counter = jobGroup->m_Counter.fetch_sub(1, std::memory_order::relaxed) - 1;

		// The fiber called us has executed the last job in the group
		if(counter == 0) {
			auto* nextGroup = jobGroup->m_NextGroup;

			if(nextGroup) {
				g_Context->EnqueueGroup(nextGroup);
			}

			if(jobGroup->m_FenceEvent) {
				jobGroup->m_FenceEvent.Signal();
			}
			delete jobGroup;
		}
	}

	// Enqueue job group for executing
	void EnqueueGroup(JobGroup* inGroup) {
		Assert(inGroup && !inGroup->m_Jobs.empty());

		for(auto& job : inGroup->m_Jobs) {
			{
				OPTICK_EVENT("Releasing a job");
				g_Context->m_ReadyJobListSync.Push(&job);
				AwakenAll();
			}
		}
	}

	// Creates graph of jobs combined into groups for parallel execution
	void BuildGraph(const JobSystem::Builder& inBuilder) {
		Assert(!inBuilder.m_Jobs.empty() && "Submitted graph is empty!");

		auto jobIt = inBuilder.m_Jobs.begin();
		auto fenceIt = inBuilder.m_Fences.begin();

		JobGroup* outGroupPredecessor = nullptr;
		// First group in the graph
		JobGroup* outGroupHead = nullptr;

		while(jobIt != inBuilder.m_Jobs.end()) {
			const auto inGroupCounterID = jobIt->m_GroupCounterID;
			auto* newGroup = new JobGroup();
			auto& outGroup = *newGroup;
			auto& outJobs = outGroup.m_Jobs;

			for(; jobIt != inBuilder.m_Jobs.end() && jobIt->m_GroupCounterID == inGroupCounterID; ++jobIt) {
				auto& outJob = outJobs.emplace_back();
				outJob.m_DebugName = jobIt->m_DebugName;
				outJob.m_JobCallable = jobIt->m_JobCallable;
				outJob.m_ParentGroup = &outGroup;

				Assert(outJob.m_JobCallable && "Job callable is empty!");
			}
			outGroup.m_Counter.store(outJobs.size(), std::memory_order::relaxed);

			if(outGroupPredecessor) {
				outGroupPredecessor->m_NextGroup = &outGroup;
			} else {
				outGroupHead = newGroup;
			}
			outGroupPredecessor = &outGroup;

			if(fenceIt != inBuilder.m_Fences.end() && fenceIt->m_PredecessorGroupCounterID == inGroupCounterID) {
				// User has destroyed the event, so ignore
				if(fenceIt->m_Event.GetRefCount() != 1) {
					outGroup.m_FenceEvent = fenceIt->m_Event;
				}
				++fenceIt;
			}
		}
		EnqueueGroup(outGroupHead);
	}

private:

	// Request exit of all threads
	std::atomic_bool					m_Exit;

	SynchronizedQueue<Job*>				m_ReadyJobListSync;
	std::list<std::thread>				m_WorkerThreads;

	// Pool of free fibers
	std::vector<Fiber>					m_FibersPool;
	SynchronizedQueue<Fiber*>			m_FibersFree;
	SynchronizedQueue<Fiber*>			m_ResumedFibers;

	// When there's no awailable work, thread go to sleep and awakened by this object
	Windows::HANDLE						m_ReadyJobSemaphore;
};

/*
* User space fiber synchronization event
* A fiber can wait for an event until it's signalled from another fiber
* Refcounted, referenced hold by notifier and notifee
*/
class Event {
public:

	Event() : m_WaitFiberList(), mb_Signalled(false) {}

	void Incref() {
		m_RefCounter.fetch_add(1, std::memory_order::relaxed);
	}

	void Decref() {
		const auto count = m_RefCounter.fetch_sub(1, std::memory_order::relaxed);

		if(count == 1) {
			Assert(m_WaitFiberList.empty());
			delete this;
		}
	}

	u32 GetRefCount() const {
		return m_RefCounter.load(std::memory_order::relaxed);
	}

	void Signal() {
		{
			Spinlock::ScopedLock lock(m_Mutex);

			if(mb_Signalled) {
				return;
			}
			mb_Signalled = true;
		}
		ResumeFibers();
	}

	bool IsSignalled() const {
		Spinlock::ScopedLock lock(m_Mutex);
		return mb_Signalled;
	}

	void ResumeFibers() {
		for(auto fiberit = m_WaitFiberList.begin(); fiberit != m_WaitFiberList.end(); ++fiberit) {
			g_Context->ResumeFiber(*fiberit);
		}
		m_WaitFiberList.clear();
	}

	bool PushWaitingFiber(Fiber* inFiber) {
		Spinlock::ScopedLock lock(m_Mutex);

		if(mb_Signalled) {
			return false;
		}
#if DEBUG 
		auto it = std::ranges::find(m_WaitFiberList, inFiber);
		Assert(it == m_WaitFiberList.end() && "This fiber is already waiting");
#endif
		m_WaitFiberList.push_back(inFiber);
		return true;
	}

private:
	mutable Spinlock	m_Mutex;
	std::list<Fiber*>	m_WaitFiberList;
	bool				mb_Signalled;

	std::atomic<u32>	m_RefCounter;
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

	Windows::SetThreadDescription(Windows::GetCurrentThread(), Util::ToWideString(threadDebugName).c_str());
	OPTICK_THREAD(inThreadDebugName.data());
	
	ThreadContext context;
	context.MasterFiber = Windows::ConvertThreadToFiber(nullptr);
	Assert(context.MasterFiber != NULL && "Cannot create master fiber");
	LOGF("Thread '{}' has started.", threadDebugName);	

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
			LOGF("Thread '{}' took a fiber '{}'.", inThreadDebugName, fiber->FiberIndex);
		}

		// Process fiber return codes
		for(;;) {
			fiber->SwitchTo(&context);

			if(context.SwitchCode == FiberSwitchCode::JobComplete) {
				LOGF("Thread '{}' has completed a job '{}'.", inThreadDebugName, context.Job->m_DebugName);
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
	LOGF("Thread '{}' exits.", inThreadDebugName);
}

// Worker Fiber
void WINAPI WorkerFiberProc(Windows::LPVOID inParameter) {
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
	LOGF("job system destructor has been called.");
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
	g_Context->GetCurrentFiber()->WaitForEvent(*inEventRef.m_Event);
}

EventRef JobSystem::CreateEvent() {
	// Will be deleted by EventRef RAII object
	auto eventObject = new Event();
	return EventRef(eventObject);
}

EventRef Builder::PushFence() {
	Assert(!m_Jobs.empty());
	auto& fence = m_Fences.emplace_back();
	++m_GroupCounter;
	auto fenceEvent = new Event();
	fence.m_Event = fenceEvent;
	return EventRef(fenceEvent);
}

EventRef::EventRef(Event* inEvent)
	: m_Event(inEvent)
{
	if(m_Event) {
		m_Event->Incref();
	}
}

EventRef::~EventRef() {
	if(!m_Event) return;
	m_Event->Decref();	
}

void EventRef::Signal() const {
	Assert(m_Event && "Reference is null");
	m_Event->Signal();
}

EventRef::EventRef(const EventRef& other)
	: m_Event(other.m_Event) {
	if(!m_Event) return;
	m_Event->Incref();
}

EventRef& EventRef::operator= (const EventRef& right) {
	if(!right.m_Event) return *this;
	if(m_Event) m_Event->Decref();
	m_Event = right.m_Event;
	m_Event->Incref();
	return *this;
}

bool EventRef::IsSignalled() const {
	Assert(m_Event && "Reference is null");
	return m_Event->IsSignalled();
}

u32 EventRef::GetRefCount() const {
	Assert(m_Event && "Reference is null");
	return m_Event->GetRefCount();
}

void EventRef::Reset() {
	if(m_Event) m_Event->Decref();
	m_Event = nullptr;
}

JOBSYSTEM_NAMESPACE_END