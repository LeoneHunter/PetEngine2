#pragma once
#include <cstdlib>
#include <Assert.h>
#include <format>
#include <iostream>

#include "types.h"

#define LOGF(fmt, ...) std::cout << std::format("[{}] : {}", __FUNCTION__, std::format(fmt, __VA_ARGS__)) << std::endl;

#define Assert(x)                                               \
	do {                                                        \
		if(!(x)) {                                              \
			LOGF("Assert failed! Condition '{}' was false.", #x); \
			/*Flush logs here*/ __debugbreak();                 \
			std::abort();                                       \
		}                                                       \
	} while(0)

#define Assertm(x, msg)                                                   \
	do {                                                                  \
		if(!(x)) {                                                        \
			LOGF("Assert failed! Condition '{}' was false. {}", #x, msg); \
			/*Flush logs here*/ __debugbreak();                           \
			std::abort();                                       		  \
		}                                                                 \
	} while(0)

#define Assertf(x, fmt, ...) Assertm(x, std::format(fmt, __VA_ARGS__)) 

#define PLATFORM_BREAK() (__debugbreak())
//#define PLATFORM_BREAK() 

#define DEBUG_BREAK_CONDITIONAL(cond)			\
	if(!!(cond)) {			\
		PLATFORM_BREAK();	\
	} else					\
		((void)0)