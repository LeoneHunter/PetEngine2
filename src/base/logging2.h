#pragma once

#include <chrono>
#include <fstream>
#include <source_location>
#include <string>
#include <string_view>

#include <task/channel.h>

enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

struct LogMessage {
  LogLevel level;
  std::string message;
  std::chrono::system_clock::time_point timestamp;
  std::source_location location;

  LogMessage(LogLevel lvl,
             std::string msg,
             std::source_location loc = std::source_location::current())
      : level(lvl)
      , message(std::move(msg))
      , timestamp(std::chrono::system_clock::now())
      , location(loc) {}
};

class Logger {
public:
  Logger() = default;

  ~Logger() { Stop(); }

public:
  void Start(std::string_view filename,
             LogLevel min_level = LogLevel::INFO,
             size_t channel_capacity = 1000) {
    file_.open(filename.data(), std::ios::app);
    DASSERT(file.is_open());

    minLevel_ = min_level;
    auto [sendChan_, receiveChan] = MakeChannel<LogMessage>(channel_capacity);
    thread_ = std::jthread(&Logger::FileThread, this, receiveChan);
  }

  void Stop() {
    DASSERT(!sendChan_.IsClosed());
    sendChan_.Close();
    if (thread_.joinable()) {
      thread_.join();
    }
    file_.close();
  }

public:
  void Log(
    LogLevel level,
    std::string_view message,
    const std::source_location& location = std::source_location::current()) {
    if (level >= minLevel_) {
      sendChan_.Send(LogMessage{level, std::string(message), location});
    }
  }

  // Convenience methods
  void Debug(
    std::string_view message,
    const std::source_location& location = std::source_location::current()) {
    Log(LogLevel::DEBUG, message, location);
  }

  void Info(
    std::string_view message,
    const std::source_location& location = std::source_location::current()) {
    Log(LogLevel::INFO, message, location);
  }

  void Warning(
    std::string_view message,
    const std::source_location& location = std::source_location::current()) {
    Log(LogLevel::WARNING, message, location);
  }

  void Error(
    std::string_view message,
    const std::source_location& location = std::source_location::current()) {
    Log(LogLevel::ERROR, message, location);
  }

  void Critical(
    std::string_view message,
    const std::source_location& location = std::source_location::current()) {
    Log(LogLevel::CRITICAL, message, location);
  }

private:
  void FileThread(ChannelReceiveEnd<LogMessage> receiveChan) {
    // Blocks while the channle is open
    // nullopt when the channel is closed
    while (auto res = receiveChan.Receive()) {
      auto msg = res.value();
      if (msg.level >= minLevel_) {
        WriteFile(msg);
      }
    }
  }

  void WriteFile(const LogMessage& msg) {
    if (!file_.is_open())
      return;

    const auto time = std::chrono::system_clock::to_time_t(msg.timestamp);

    file_ << std::put_time(std::localtime(&time), "%F %T") << " ["
          << LevelToString(msg.level) << "] " << msg.location.file_name()
          << ":" << msg.location.line() << " - " << msg.message << std::endl;
  }

  static constexpr std::string_view LevelToString(LogLevel level) {
    switch (level) {
      case LogLevel::DEBUG: return "DEBUG";
      case LogLevel::INFO: return "INFO";
      case LogLevel::WARNING: return "WARNING";
      case LogLevel::ERROR: return "ERROR";
      case LogLevel::CRITICAL: return "CRITICAL";
      default: return "UNKNOWN";
    }
  }

private:
  ChannelSendEnd<LogMessage> sendChan_;
  std::ofstream file_;
  std::jthread thread_;
  LogLevel minLevel_;
};

// Global logger instance
inline Logger& get_logger() {
  static Logger logger;
  return logger;
}

// Macros for convenient logging (automatically captures source location)
#define LOG_DEBUG(msg) get_logger().Debug(msg)
#define LOG_INFO(msg) get_logger().Info(msg)
#define LOG_WARNING(msg) get_logger().Warning(msg)
#define LOG_ERROR(msg) get_logger().Error(msg)
#define LOG_CRITICAL(msg) get_logger().Critical(msg)