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
add_compile_options(/W3)

# Find files on Configure
file(GLOB_RECURSE FOUND_FILES
  src/editor/*.cpp
  src/editor/*.h
  src/runtime/*.cpp
  src/runtime/*.h
  src/thirdparty/*.cpp
  src/thirdparty/*.h
)

# Files with "_test.*" go into test project
# Other files into main project
foreach(FILE ${FOUND_FILES})
    get_filename_component(FILENAME "${FILE}" NAME_WLE)
    string(FIND ${FILENAME} "_test" IS_TEST_FILE)

    if(NOT ${IS_TEST_FILE} EQUAL -1)
        set(TEST_FILES ${TEST_FILES} ${FILE})
    else()
        set(MAIN_FILES ${MAIN_FILES} ${FILE})
    endif()
endforeach()

add_subdirectory(src)
add_subdirectory(test)