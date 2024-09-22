cmake_minimum_required(VERSION 3.29)

project(PetEngine2 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")


# =======================================================
# Options
# =======================================================
option(USE_TEST_HOST "Combine all test files into a single executable 'test_host'" OFF)
option(ENABLE_ASSERTS "Force asserts in all builds" OFF)


# =======================================================
# Global target with common settings
# =======================================================
add_library(common_config INTERFACE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(common_config
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/thirdparty"
)

if(ENABLE_ASSERTS)
    target_compile_definitions(common_config INTERFACE IS_DEBUG_BUILD)
else()
    target_compile_definitions(common_config INTERFACE $<$<CONFIG:Debug>:IS_DEBUG_BUILD>)
endif()

target_compile_definitions(common_config
    INTERFACE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

target_link_options(common_config
    INTERFACE
    # Link time optimizaionts
    $<$<NOT:$<CONFIG:Debug>>: /OPT:REF /OPT:ICF>
    # suppress linking warning due to /INCREMENTAL and /OPT:ICF being both ON
    $<$<CONFIG:RelWithDebInfo>: /INCREMENTAL:NO>
    # Ignore .pdb missing for third party libs
    /ignore:4099
)

target_compile_options(common_config INTERFACE
    # Enables standard C++ stack unwinding.
    # Catches only standard C++ exceptions when you use catch(...) syntax.
    # Unless /EHc is also specified, the compiler assumes that functions
    # declared as extern "C" may throw a C++ exception.
    /EHsc
    # Optimizations in release builds
    $<$<NOT:$<CONFIG:Debug>>: /O2 /Zc:inline>
    # -Wall
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
    # /wd4189
    # declaration of 'identifier' hides class member.
    # class members is suffixed with _ so this shouldn't be a problem
    # detected in tests
    # /wd4458
    # conditional expression is constant
    /wd4127
    # declaration of 'identifier' hides local declaration
    # /wd4456
    # unreachable code
    /wd4702
    # conversion, possible loss of data (in msvc headers)
    /wd4244
    # narrowing conversion, possible loss of data
    /wd4267
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|x86_64|AMD64|amd64|i386|i686")
    set(HAVE_NEON FALSE)
    set(HAVE_AVX ON)
    set(AVX_COMPILE_FLAGS "/arch:AVX")
    target_compile_definitions(common_config INTERFACE "HAVE_AVX")

    set(HAVE_AVX2 ON)
    set(AVX2_COMPILE_FLAGS "/arch:AVX2")
    target_compile_definitions(common_config INTERFACE "HAVE_AVX2")

    set(HAVE_AVX512F ON)
    set(AVX512F_COMPILE_FLAGS "/arch:AVX512")
    target_compile_definitions(common_config INTERFACE "HAVE_AVX512F")

    set(HAVE_SSE4_1 ON)
    set(SSE4_1_COMPILE_FLAGS "-D__SSE4_1__")
    target_compile_definitions(common_config INTERFACE "HAVE_SSE4_1")
endif()


# =============================================================
# Adds a module (library) to a project
# Mimics Bazel cc_library
#
# Parameters:
#   NAME: name of target
#   HDRS: List of public headers
#   SRCS: List of source files
#   DEPS: List of targets this target depends on (will be linked against)
#   COPTS: List of private compile options
#   DEFINES: List of public defines
#   LINKOPTS: List of link options
#
# module(
#   NAME
#       my_module
#   HDRS
#       "header.h"
#   SRCS
#       "source.cpp"
# )
# ===============================================================
function(module)
    set(options "")
    set(oneValueArgs "NAME;ALIAS")
    set(multiValueArgs "HDRS;SRCS;COPTS;DEFINES;LINKOPTS;DEPS")
    cmake_parse_arguments(ARG
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if("${ARG_SRCS}" STREQUAL "")
        set(MODULE_INTERFACE 1)
    else()
        set(MODULE_INTERFACE 0)
    endif()

    if(NOT ${MODULE_INTERFACE})
        add_library(${ARG_NAME} STATIC "")
        target_sources(${ARG_NAME}
            PRIVATE
            ${ARG_SRCS}
            ${ARG_HDRS}
        )
        target_link_libraries(${ARG_NAME}
            PUBLIC
            ${ARG_DEPS}
            PRIVATE
            ${ARG_LINKOPTS}
        )
        target_link_libraries(${ARG_NAME}
            PRIVATE
            common_config
        )
        target_compile_definitions(${ARG_NAME}
            PUBLIC
            ${ARG_DEFINES}
        )
        target_compile_options(${ARG_NAME}
            PRIVATE
            ${ARG_COPTS}
        )
        target_include_directories(${ARG_NAME}
            PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        )
    else()
        # Header-only module
        add_library(${ARG_NAME} INTERFACE)
        target_link_libraries(${ARG_NAME}
            INTERFACE
            ${ARG_DEPS}
            ${ARG_LINKOPTS}
        )
        target_compile_definitions(${ARG_NAME}
            INTERFACE
            ${ABSL_CC_LIB_DEFINES}
        )
        target_include_directories(${ARG_NAME}
            INTERFACE
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        )
    endif()

    if(NOT ${ARG_ALIAS} STREQUAL "")
        add_library("${ARG_ALIAS}" ALIAS "${ARG_NAME}")
    endif()
endfunction()


# =============================================
# Same as |library| but creates an executable
# =============================================
function(executable)
    set(options "")
    set(oneValueArgs "NAME")
    set(multiValueArgs "HDRS;SRCS;COPTS;DEFINES;LINKOPTS;DEPS")
    cmake_parse_arguments(ARG
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    add_executable(${ARG_NAME})
    target_sources(${ARG_NAME} PRIVATE ${ARG_SRCS} ${ARG_HDRS})
    target_link_libraries(${ARG_NAME} PRIVATE common_config)
    target_link_libraries(${ARG_NAME} PUBLIC ${ARG_DEPS} PRIVATE ${ARG_LINKOPTS})
    target_compile_options(${ARG_NAME} PRIVATE ${ARG_COPTS})
    set_target_properties(${ARG_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG
        ${CMAKE_SOURCE_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE
        ${CMAKE_SOURCE_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
        ${CMAKE_SOURCE_DIR}/bin
        OUTPUT_NAME
        ${ARG_NAME}$<$<CONFIG:Debug>:_d>
    )
endfunction()

# =======================================================
# Global test target with test sources
# test_host.exe
# =======================================================
function(create_test_host)
    add_executable(test_host)
    target_sources(test_host PRIVATE src/test_main.cpp)
    target_link_libraries(test_host PRIVATE common_config)
    target_include_directories(test_host
        PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/doctest
    )
    set_target_properties(test_host PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG
        ${CMAKE_SOURCE_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE
        ${CMAKE_SOURCE_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
        ${CMAKE_SOURCE_DIR}/bin
        OUTPUT_NAME
        test_host$<$<CONFIG:Debug>:_d>
    )
    add_test(NAME tests COMMAND test_host)
endfunction()

if(USE_TEST_HOST)
    create_test_host()
endif()


# ======================================================
# Adds files to test target
# Only source files (.cpp)
# Parameters:
#   NAME: name of the test
#   SRCS: List of source files
#   DEPS: List of targets this target depends on (will be linked against)
#   DEFINES: List of public defines
# ======================================================
function(test)
    set(options "")
    set(oneValueArgs "NAME")
    set(multiValueArgs "SRCS;DEFINES;DEPS")
    cmake_parse_arguments(ARG
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )
    set(TEST_NAME "test_${ARG_NAME}")

    if(USE_TEST_HOST)
        target_sources(test_host PRIVATE ${ARG_SRCS})
        target_link_libraries(test_host PUBLIC ${ARG_DEPS} PRIVATE ${ARG_LINKOPTS})
        target_compile_definitions(test_host PUBLIC ${ARG_DEFINES})
    else()
        executable(
            NAME 
                ${TEST_NAME}
            SRCS
                ${CMAKE_SOURCE_DIR}/src/test_main.cpp
                ${ARG_SRCS}
            DEPS
                ${ARG_DEPS}
        )
        target_include_directories(${TEST_NAME}
            PRIVATE
            ${CMAKE_SOURCE_DIR}/thirdparty/doctest
        )
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endif()
endfunction()

add_subdirectory(src)
add_subdirectory(thirdparty)