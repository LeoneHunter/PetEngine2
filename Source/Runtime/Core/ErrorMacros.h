#pragma once
#include <cstdlib>
#include <Assert.h>
#include <format>
#include <iostream>

#include "BaseTypes.h"

#define Assert(x) do { if(!(x)) { /*Flush logs here*/ __debugbreak(); assert(x); } } while(0)

#define LOGF(fmt, ...) std::cout << std::format("[{}] : {}", __FUNCTION__, std::format(fmt, __VA_ARGS__)) << std::endl;

#define Assertm(x, msg) do { if(!(x)) { LOGF(msg); __debugbreak(); assert(x); } } while(0)

#define Assertf(x, fmt, ...) do { if(!(x)) { LOGF(fmt, __VA_ARGS__); __debugbreak(); assert(x); } } while(0)

#define PLATFORM_BREAK() (__debugbreak())
//#define PLATFORM_BREAK() 

#define DEBUG_BREAK_CONDITIONAL(cond)			\
	if(!!(cond)) {			\
		PLATFORM_BREAK();	\
	} else					\
		((void)0)