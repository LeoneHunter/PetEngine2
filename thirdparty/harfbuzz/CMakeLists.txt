
cmake_minimum_required(VERSION 3.12)
project(harfbuzz)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch the source
FetchContent_Declare(
    harfbuzz
    GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/harfbuzz"
    GIT_TAG "10.0.1"
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(harfbuzz)

if(NOT harfbuzz_POPULATED)
    FetchContent_Populate(harfbuzz)
endif()

# Include freetype
if(NOT TARGET freetype)
    message(FATAL_ERROR "\"freetype\" target not found")
endif()

add_definitions(
    -DHAVE_FREETYPE

    # Use internal ICU tables
    -DHAVE_ICU_BUILTIN
    -DHAVE_OT

    # Use only internal shaper
    -DHB_NO_FALLBACK_SHAPE

    # Disable .fon files
    -DHB_NO_WIN1256
)

# # Functions and headers
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)

macro(check_funcs) # Similar to AC_CHECK_FUNCS of autotools
    foreach(func_name ${ARGN})
        string(TOUPPER ${func_name} definition_to_add)
        check_function_exists(${func_name} HAVE_${definition_to_add})

        if(${HAVE_${definition_to_add}})
            add_definitions(-DHAVE_${definition_to_add})
        endif()
    endforeach()
endmacro()

check_funcs(atexit mprotect sysconf getpagesize mmap isatty)
check_include_file(unistd.h HAVE_UNISTD_H)

if(${HAVE_UNISTD_H})
    add_definitions(-DHAVE_UNISTD_H)
endif()

check_include_file(sys/mman.h HAVE_SYS_MMAN_H)

if(${HAVE_SYS_MMAN_H})
    add_definitions(-DHAVE_SYS_MMAN_H)
endif()

check_include_file(stdbool.h HAVE_STDBOOL_H)

if(${HAVE_STDBOOL_H})
    add_definitions(-DHAVE_STDBOOL_H)
endif()

add_definitions(
    -wd4244
    -wd4267
    -wd4701
    -wd4706
    -wd4457
    -wd4310
    -wd4309
    -wd4245
    -wd4459
    -D_CRT_SECURE_NO_WARNINGS
    -D_CRT_NONSTDC_NO_WARNINGS
)
add_definitions(-bigobj)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/harfbuzz/src)

file(READ ${SOURCE_DIR}/meson.build MESONBUILD)
string(REGEX MATCH "version: '(([0-9]+)\\.([0-9]+)\\.([0-9]+))'," HB_VERSION_MATCH ${MESONBUILD})
set(HB_VERSION ${CMAKE_MATCH_1})
set(HB_VERSION_MAJOR ${CMAKE_MATCH_2})
set(HB_VERSION_MINOR ${CMAKE_MATCH_3})
set(HB_VERSION_MICRO ${CMAKE_MATCH_4})

# Define sources and headers of the project
# use amalgam source
set(project_sources ${SOURCE_DIR}/harfbuzz.cc)

if(ALL_SOURCES)
    set(project_sources
        ${SOURCE_DIR}/OT/Var/VARC/VARC.cc
        ${SOURCE_DIR}/hb-aat-layout.cc
        ${SOURCE_DIR}/hb-aat-map.cc
        ${SOURCE_DIR}/hb-blob.cc
        ${SOURCE_DIR}/hb-buffer-serialize.cc
        ${SOURCE_DIR}/hb-buffer-verify.cc
        ${SOURCE_DIR}/hb-buffer.cc
        ${SOURCE_DIR}/hb-common.cc
        ${SOURCE_DIR}/hb-coretext.cc
        ${SOURCE_DIR}/hb-directwrite.cc
        ${SOURCE_DIR}/hb-draw.cc
        ${SOURCE_DIR}/hb-face-builder.cc
        ${SOURCE_DIR}/hb-face.cc
        ${SOURCE_DIR}/hb-fallback-shape.cc
        ${SOURCE_DIR}/hb-font.cc
        ${SOURCE_DIR}/hb-ft.cc
        ${SOURCE_DIR}/hb-gdi.cc
        ${SOURCE_DIR}/hb-glib.cc
        ${SOURCE_DIR}/hb-graphite2.cc
        ${SOURCE_DIR}/hb-map.cc
        ${SOURCE_DIR}/hb-number.cc
        ${SOURCE_DIR}/hb-ot-cff1-table.cc
        ${SOURCE_DIR}/hb-ot-cff2-table.cc
        ${SOURCE_DIR}/hb-ot-color.cc
        ${SOURCE_DIR}/hb-ot-face.cc
        ${SOURCE_DIR}/hb-ot-font.cc
        ${SOURCE_DIR}/hb-ot-layout.cc
        ${SOURCE_DIR}/hb-ot-map.cc
        ${SOURCE_DIR}/hb-ot-math.cc
        ${SOURCE_DIR}/hb-ot-meta.cc
        ${SOURCE_DIR}/hb-ot-metrics.cc
        ${SOURCE_DIR}/hb-ot-name.cc
        ${SOURCE_DIR}/hb-ot-shape-fallback.cc
        ${SOURCE_DIR}/hb-ot-shape-normalize.cc
        ${SOURCE_DIR}/hb-ot-shape.cc
        ${SOURCE_DIR}/hb-ot-shaper-arabic.cc
        ${SOURCE_DIR}/hb-ot-shaper-default.cc
        ${SOURCE_DIR}/hb-ot-shaper-hangul.cc
        ${SOURCE_DIR}/hb-ot-shaper-hebrew.cc
        ${SOURCE_DIR}/hb-ot-shaper-indic-table.cc
        ${SOURCE_DIR}/hb-ot-shaper-indic.cc
        ${SOURCE_DIR}/hb-ot-shaper-khmer.cc
        ${SOURCE_DIR}/hb-ot-shaper-myanmar.cc
        ${SOURCE_DIR}/hb-ot-shaper-syllabic.cc
        ${SOURCE_DIR}/hb-ot-shaper-thai.cc
        ${SOURCE_DIR}/hb-ot-shaper-use.cc
        ${SOURCE_DIR}/hb-ot-shaper-vowel-constraints.cc
        ${SOURCE_DIR}/hb-ot-tag.cc
        ${SOURCE_DIR}/hb-ot-var.cc
        ${SOURCE_DIR}/hb-outline.cc
        ${SOURCE_DIR}/hb-paint-extents.cc
        ${SOURCE_DIR}/hb-paint.cc
        ${SOURCE_DIR}/hb-set.cc
        ${SOURCE_DIR}/hb-shape-plan.cc
        ${SOURCE_DIR}/hb-shape.cc
        ${SOURCE_DIR}/hb-shaper.cc
        ${SOURCE_DIR}/hb-static.cc
        ${SOURCE_DIR}/hb-style.cc
        ${SOURCE_DIR}/hb-ucd.cc
        ${SOURCE_DIR}/hb-unicode.cc
        ${SOURCE_DIR}/hb-uniscribe.cc
        ${SOURCE_DIR}/hb-wasm-api.cc
        ${SOURCE_DIR}/hb-wasm-shape.cc
    )
endif()

set(subset_project_sources
    ${SOURCE_DIR}/hb-number.cc
    ${SOURCE_DIR}/hb-number.hh
    ${SOURCE_DIR}/hb-ot-cff1-table.cc
    ${SOURCE_DIR}/hb-ot-cff2-table.cc
    ${SOURCE_DIR}/hb-ot-post-table-v2subset.hh
    ${SOURCE_DIR}/hb-static.cc
    ${SOURCE_DIR}/hb-subset-cff-common.cc
    ${SOURCE_DIR}/hb-subset-cff-common.hh
    ${SOURCE_DIR}/hb-subset-cff1.cc
    ${SOURCE_DIR}/hb-subset-cff2.cc
    ${SOURCE_DIR}/hb-subset-input.cc
    ${SOURCE_DIR}/hb-subset-input.hh
    ${SOURCE_DIR}/hb-subset-instancer-iup.hh
    ${SOURCE_DIR}/hb-subset-instancer-iup.cc
    ${SOURCE_DIR}/hb-subset-instancer-solver.hh
    ${SOURCE_DIR}/hb-subset-instancer-solver.cc
    ${SOURCE_DIR}/hb-subset-accelerator.hh
    ${SOURCE_DIR}/hb-subset-plan.cc
    ${SOURCE_DIR}/hb-subset-plan.hh
    ${SOURCE_DIR}/hb-subset-plan-member-list.hh
    ${SOURCE_DIR}/hb-subset-repacker.cc
    ${SOURCE_DIR}/hb-subset.cc
    ${SOURCE_DIR}/hb-subset.hh
    ${SOURCE_DIR}/hb-repacker.hh
    ${SOURCE_DIR}/graph/graph.hh
    ${SOURCE_DIR}/graph/gsubgpos-graph.hh
    ${SOURCE_DIR}/graph/gsubgpos-context.hh
    ${SOURCE_DIR}/graph/gsubgpos-context.cc
    ${SOURCE_DIR}/graph/coverage-graph.hh
    ${SOURCE_DIR}/graph/classdef-graph.hh
    ${SOURCE_DIR}/graph/pairpos-graph.hh
    ${SOURCE_DIR}/graph/markbasepos-graph.hh
    ${SOURCE_DIR}/graph/split-helpers.hh
    ${SOURCE_DIR}/graph/serialize.hh
    ${SOURCE_DIR}/OT/Color/COLR/colrv1-closure.hh
)
set(project_headers
    ${SOURCE_DIR}/hb-aat-layout.h
    ${SOURCE_DIR}/hb-aat.h
    ${SOURCE_DIR}/hb-blob.h
    ${SOURCE_DIR}/hb-buffer.h
    ${SOURCE_DIR}/hb-common.h
    ${SOURCE_DIR}/hb-cplusplus.hh
    ${SOURCE_DIR}/hb-deprecated.h
    ${SOURCE_DIR}/hb-draw.h
    ${SOURCE_DIR}/hb-face.h
    ${SOURCE_DIR}/hb-font.h
    ${SOURCE_DIR}/hb-map.h
    ${SOURCE_DIR}/hb-ot-color.h
    ${SOURCE_DIR}/hb-ot-deprecated.h
    ${SOURCE_DIR}/hb-ot-font.h
    ${SOURCE_DIR}/hb-ot-layout.h
    ${SOURCE_DIR}/hb-ot-math.h
    ${SOURCE_DIR}/hb-ot-meta.h
    ${SOURCE_DIR}/hb-ot-metrics.h
    ${SOURCE_DIR}/hb-ot-name.h
    ${SOURCE_DIR}/hb-ot-shape.h
    ${SOURCE_DIR}/hb-ot-var.h
    ${SOURCE_DIR}/hb-ot.h
    ${SOURCE_DIR}/hb-paint.h
    ${SOURCE_DIR}/hb-set.h
    ${SOURCE_DIR}/hb-shape-plan.h
    ${SOURCE_DIR}/hb-shape.h
    ${SOURCE_DIR}/hb-style.h
    ${SOURCE_DIR}/hb-unicode.h
    ${SOURCE_DIR}/hb-version.h
    ${SOURCE_DIR}/hb.h
)
set(subset_project_headers
    ${SOURCE_DIR}/hb-subset.h
    ${SOURCE_DIR}/hb-subset-repacker.h
)

list(APPEND THIRD_PARTY_LIBS ${FREETYPE_LIBRARIES})
include_directories(AFTER ${FREETYPE_INCLUDE_DIRS})

list(APPEND project_headers ${SOURCE_DIR}/hb-ft.h)

# So check_funcs can find its headers
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${FREETYPE_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${FREETYPE_LIBRARIES})

check_funcs(FT_Get_Var_Blend_Coordinates FT_Set_Var_Blend_Coordinates FT_Done_MM_Var)

# =======================================================
# Harfbuzz library
# =======================================================
add_library(harfbuzz ${project_sources} ${project_headers})
target_link_libraries(harfbuzz ${THIRD_PARTY_LIBS})

target_include_directories(harfbuzz
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/harfbuzz/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/harfbuzz/src>
)
target_link_libraries(harfbuzz freetype)

target_compile_options(harfbuzz PUBLIC /wd4244 /wd4267)
