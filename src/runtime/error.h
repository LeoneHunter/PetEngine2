#pragma once
#include <format>
#include "log.h"

#ifndef NDEBUG
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
#endif  // NDEBUG

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
