#include "log.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <deque>
#include <semaphore>

#include "threading.h"

namespace logging {

constexpr auto kBufferSize = 100;

void LogProc();

struct Context {
	std::string				logDir;
	Level                   level;
	std::thread             thread;
	std::binary_semaphore   sema;
	std::atomic_bool        bShouldExit;
	std::atomic_bool		bFlushFile;
	std::deque<Record>		queue;
	Spinlock				lock;
};
Context* ctx = nullptr;

void Init(const std::string& logDirectory) {
	if(ctx) return;
	ctx = new Context{
		.logDir = logDirectory,
		.level = Level::All,
		.thread = std::thread(LogProc),
		.sema = std::binary_semaphore(0),
		.bShouldExit = false,
		.bFlushFile = false,
	};
}

void Shutdown() {
	if(!ctx) return;
	ctx->bFlushFile.store(true, std::memory_order::relaxed);
	ctx->bShouldExit.store(true, std::memory_order::relaxed);
	ctx->sema.release();
	ctx->thread.join();
}

void Flush() {
	ctx->bFlushFile.store(true, std::memory_order::relaxed);
	ctx->sema.release();
}

void SetLevel(Level level) {
	ctx->level = level;
}

bool ShouldLog(Level level) {
	return level <= ctx->level;
}

void DoLog(Record&& record) {
	{
		Spinlock::ScopedLock _{ctx->lock};
		ctx->queue.push_back(std::move(record));
	}
	if(record.level == Level::Fatal) {
		// Block until flushed
		Shutdown();
		return;
	}
	ctx->sema.release();
}

void LogProc() {
	//auto timeStamp = FormatTime(std::chrono::system_clock::now(), true, false, true);
	auto logFilename = std::string(ctx->logDir).append("\\log.txt");
	std::ofstream logFile(logFilename);
	if (logFile.fail()) { std::abort(); }

	LOGF(Verbose, "Log file {} has been opened.", logFilename);

	// Start processing loop.
	while (true) {
		ctx->sema.acquire();

		while(true) {
			Record rec;
			{
				Spinlock::ScopedLock _(ctx->lock);
				if(ctx->queue.empty()) 
					break;
				rec = ctx->queue.front();
				ctx->queue.pop_front();
			}
			auto msg = std::format(
					"{}:{} [{}]: {}",
					rec.file,
					rec.line,
					to_string(rec.level),
					rec.message
			);
			logFile << msg << '\n';
			std::cout << msg << std::endl;
		}
		if(ctx->bFlushFile.load(std::memory_order::relaxed)) {
			logFile.flush();
			ctx->bFlushFile.store(false, std::memory_order::relaxed);
		}
		if(ctx->bShouldExit.load(std::memory_order::relaxed)) {
			break;
		}
	}
}
}