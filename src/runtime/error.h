#pragma once
#include <format>

#include <Assert.h>
#include "log.h"

// TODO: Refactor: ASSERT(cond, ...), INVARIANT(cond, ...)
// - Remove in release buid

#define ASSERT_MSG(x, msg)                                                       \
	do {                                                                         \
		if(!(x)) {                                                               \
			LOGF(Fatal, "Assert failed! {}", msg); \
			/*Flush logs here*/ __debugbreak();                                  \
			std::abort();                                                        \
		}                                                                        \
	} while(0)

#define Assert(x)                                                        \
	do {                                                                 \
		if(!(x)) {                                                       \
			LOGF(Fatal, "Assert failed! Condition '{}' was false.", #x); \
			/*Flush logs here*/ __debugbreak();                          \
			std::abort();                                                \
		}                                                                \
	} while(0)


#define Assertm(x, msg)                                                          \
	do {                                                                         \
		if(!(x)) {                                                               \
			LOGF(Fatal, "Assert failed! Condition '{}' was false. {}", #x, msg); \
			/*Flush logs here*/ __debugbreak();                                  \
			std::abort();                                                        \
		}                                                                        \
	} while(0)

#define Assertf(x, fmt, ...) Assertm(x, std::format(fmt, __VA_ARGS__)) 

#define PLATFORM_BREAK() (__debugbreak())
//#define PLATFORM_BREAK()

#define DEBUG_BREAK_CONDITIONAL(cond) \
	if(!!(cond)) {                    \
		PLATFORM_BREAK();             \
	} else                            \
		((void)0)


#define UNREACHABLE() ASSERT_MSG(false, "Unreachable statement has been reached!")


#ifndef NDEBUG
	#define DASSERT(cond) Assert(cond)
#else
	#define DASSERT(cond)
#endif // NDEBUG