#pragma once
#include <chrono>
#include <stacktrace>
#include <string>

void PrintToCerr(const std::string& str);

namespace logging {

enum class Level : uint8_t {
    NoLogging = 0,
    Fatal,
    Error,
    Warning,
    Info,
    Verbose,

    All = Verbose,
    _Count,
};

inline const char* to_string(Level level) {
    switch (level) {
        case Level::NoLogging: return "NoLogging";
        case Level::Fatal: return "Fatal";
        case Level::Error: return "Error";
        case Level::Warning: return "Warning";
        case Level::Info: return "Info";
        case Level::Verbose: return "Verbose";
    }
    return "Unknown";
}

using time_point = std::chrono::high_resolution_clock::time_point;

struct Record {
    Level level;
    std::string_view file;
    std::string_view func;
    uint32_t line;
    uint32_t frameNum;
    time_point timePoint;
    std::string message;
    // TODO add thread id
};

// Creates a logger thread and a file in the logDirectory
void Init(const std::string& logDirectory);
bool IsInitialized();
void Shutdown();
void Flush();
void SetLevel(Level level);
bool ShouldLog(Level level);
void DoLog(Record&& record);


template <typename... ArgTypes>
inline void Logf(std::string_view file,
                 uint32_t line,
                 std::string_view func,
                 Level level,
                 const std::format_string<ArgTypes...> fmt,
                 ArgTypes... args) {
    // If not initialized (in tests) just print to cerr
    if (!IsInitialized()) {
        // Print location for errors and crashes
        if (level < Level::Warning) {
            PrintToCerr(std::format("[{}] {}({}) in {}:", to_string(level),
                                    file, line, func));
        }
        const std::string message =
            std::format(fmt, std::forward<ArgTypes>(args)...);
        PrintToCerr(std::format("[{}] > {}", to_string(level), message));
        // Add stacktrace
        if (level == Level::Fatal) {
            PrintToCerr(std::format("[{}] Stacktrace:\n{}", to_string(level),
                                    std::stacktrace::current()));
        }
        return;
    }
    if (!ShouldLog(level)) {
        return;
    }
    DoLog(Record{.level = level,
                 .file = file,
                 .func = func,
                 .line = line,
                 .frameNum = 0,
                 .timePoint = std::chrono::high_resolution_clock::now(),
                 .message = std::format(fmt, std::forward<ArgTypes>(args)...)});
}

}  // namespace logging

#define LOGF(level, fmt, ...)                                              \
    logging::Logf(__FILE__, __LINE__, __FUNCTION__, logging::Level::level, \
                  fmt, __VA_ARGS__);

// Simple formatter cout print
template <class... Types>
inline void Println(const std::format_string<Types...> fmt, Types&&... args) {
    PrintToCerr(std::format(fmt, std::forward<Types>(args)...));
}