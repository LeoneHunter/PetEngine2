cmake_minimum_required(VERSION 3.29 FATAL_ERROR)

project(PetEngine2 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|x86_64|AMD64|amd64|i386|i686")

	set(HAVE_NEON FALSE)
	set(HAVE_AVX ON)
	set(AVX_COMPILE_FLAGS "/arch:AVX")
	add_definitions("-DHAVE_AVX")

	set(HAVE_AVX2 ON)
	set(AVX2_COMPILE_FLAGS "/arch:AVX2")
	add_definitions("-DHAVE_AVX2")

	set(HAVE_AVX512F ON)
	set(AVX512F_COMPILE_FLAGS "/arch:AVX512")
	add_definitions("-DHAVE_AVX512F")

	set(HAVE_FMA ON)
	set(FMA_COMPILE_FLAGS "-D__FMA__")
	add_definitions("-DHAVE_FMA")

	set(HAVE_SSE4_1 ON)
	set(SSE4_1_COMPILE_FLAGS "-D__SSE4_1__")
	add_definitions("-DHAVE_SSE4_1")

endif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|x86_64|AMD64|amd64|i386|i686")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_compile_options(
	/W4
	# nonstandard extension used: nameless struct/union
    /wd4201 
	# confitional is always true. used in macros and loops
	/wd4127 
	# Unreferenced formal function parameter. used in interface declarations
    /wd4100 
	# Disable warning about alignment padding
	/wd4324 
	# local variable is initialized but not referenced
    /wd4189 
	# declaration of 'identifier' hides class member. 
	# class members is suffixed with _ so this shouldn't be a problem
	# detected in tests
    /wd4458 
	# declaration of 'identifier' hides local declaration
    /wd4456
	# unreachable code
    /wd4702
)

add_subdirectory(src)