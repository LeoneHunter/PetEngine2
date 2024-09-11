#pragma once
#include "log.h"

#include <format>
#include <expected>

#if defined(IS_DEBUG_BUILD)
#define DASSERT(x)                                                       \
    do {                                                                 \
        if (!(x)) {                                                      \
            LOGF(Fatal, "Assert failed! Condition '{}' was false.", #x); \
            __debugbreak();                                              \
            std::abort();                                                \
        }                                                                \
    } while (0)

#define DASSERT_M(x, msg)                                                  \
    do {                                                                   \
        if (!(x)) {                                                        \
            LOGF(Fatal, "Assert failed! Condition '{}' was false. {}", #x, \
                 msg);                                                     \
            __debugbreak();                                                \
            std::abort();                                                  \
        }                                                                  \
    } while (0)

#define DASSERT_F(x, fmt, ...) DASSERT_M(x, std::format(fmt, __VA_ARGS__))
#else
#define DASSERT(cond)
#define DASSERT_M(x, msg)
#define DASSERT_F(x, fmt, ...)
#endif  // IS_DEBUG_BUILD

#define FATAL(...) DASSERT_F(false, __VA_ARGS__)

#define BREAK_IN_DEBUGGER_CONDITIONAL(cond) \
    if (!!(cond)) {                         \
        __debugbreak();                     \
    } else                                  \
        ((void)0)

#define UNREACHABLE()                                           \
    do {                                                        \
        LOGF(Fatal, "Unreachable statement has been reached!"); \
        __debugbreak();                                         \
        std::abort();                                           \
    } while (0)


// Generic error codes
enum class GenericErrorCode {
    Ok,
    InternalError,
    BadValue,
    NotFound,
    UnsupportedFormat,
    AlreadyInitialized,
    IndexNotFound,
    InvalidPath,
    OutOfMemory,
    AlreadyExists,
};

constexpr std::string to_string(GenericErrorCode ec) {
    switch (ec) {
        case GenericErrorCode::Ok: return "Ok";
        case GenericErrorCode::InternalError: return "InternalError";
        case GenericErrorCode::BadValue: return "BadValue";
        case GenericErrorCode::NotFound: return "NotFound";
        case GenericErrorCode::UnsupportedFormat: return "UnsupportedFormat";
        case GenericErrorCode::AlreadyInitialized: return "AlreadyInitialized";
        case GenericErrorCode::IndexNotFound: return "IndexNotFound";
        case GenericErrorCode::InvalidPath: return "InvalidPath";
        case GenericErrorCode::OutOfMemory: return "OutOfMemory";
        case GenericErrorCode::AlreadyExists: return "AlreadyExists";
        default: return "";
    }
}

template <>
struct std::formatter<GenericErrorCode> : std::formatter<std::string> {
    using Base = std::formatter<std::string>;
    using Base::parse;
    template <class T>
    auto format(const GenericErrorCode& value, T& ctx) const {
        return Base::format(to_string(value), ctx);
    }
};