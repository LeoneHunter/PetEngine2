#pragma once
#include <string>
#include <mutex>
#include <map>
#include <chrono>

#include "runtime/core/types.h"

namespace logging {

enum class Level: u8 {
	NoLogging = 0,
	Fatal,
	Error,
	Warning,
	Verbose,

	All = Verbose,
	_Count,
};

inline const char* to_string(Level level) {
	switch(level) {
		case Level::NoLogging: return "NoLogging";
		case Level::Fatal: return "Fatal";
		case Level::Error: return "Error";
		case Level::Warning: return "Warning";
		case Level::Verbose: return "Verbose";
	}
	return "Unknown";
}

using time_point = std::chrono::high_resolution_clock::time_point;

struct Record {
	Level            level;
	std::string_view file;
	std::string_view func;
	u32              line;
	u32              frameNum;
	time_point       timePoint;
	std::string      message;
	// TODO add thread id
};

// Creates a logger thread and a file in the logDirectory
void Init(const std::string& logDirectory);
void Shutdown();
void Flush();
void SetLevel(Level level);
bool ShouldLog(Level level);
void DoLog(Record&& record);

template<typename... ArgTypes>
inline void Logf(std::string_view                      file,
 				 u32                                   line,
				 std::string_view                      func,
				 Level                                 level,
				 const std::format_string<ArgTypes...> fmt,
				 ArgTypes... 						   args) {
	if (!ShouldLog(level)) return;
	DoLog(Record{
		.level = level,
		.file = file,
		.func = func,
		.line = line,
		.frameNum = 0,
		.timePoint = std::chrono::high_resolution_clock::now(),
		.message = std::format(fmt, std::forward<ArgTypes>(args)...)
	});
}

}

#define LOGF(level, fmt, ...) logging::Logf(__FILE__, __LINE__, __FUNCTION__, logging::Level::##level, fmt, __VA_ARGS__);
