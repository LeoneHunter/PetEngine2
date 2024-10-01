# Modified to be used only with MSVC/Windows/x86 for PetEngine2
cmake_minimum_required(VERSION 3.12)
project(harfbuzz)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(AFTER
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_BINARY_DIR}/src
)

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

file(READ meson.build MESONBUILD)
string(REGEX MATCH "version: '(([0-9]+)\\.([0-9]+)\\.([0-9]+))'," HB_VERSION_MATCH ${MESONBUILD})
set(HB_VERSION ${CMAKE_MATCH_1})
set(HB_VERSION_MAJOR ${CMAKE_MATCH_2})
set(HB_VERSION_MINOR ${CMAKE_MATCH_3})
set(HB_VERSION_MICRO ${CMAKE_MATCH_4})

# Define sources and headers of the project
# use amalgam source
set (project_sources ${PROJECT_SOURCE_DIR}/src/harfbuzz.cc) 

if(ALL_SOURCES) 
  set(project_sources
    ${PROJECT_SOURCE_DIR}/src/OT/Var/VARC/VARC.cc
    ${PROJECT_SOURCE_DIR}/src/hb-aat-layout.cc
    ${PROJECT_SOURCE_DIR}/src/hb-aat-map.cc
    ${PROJECT_SOURCE_DIR}/src/hb-blob.cc
    ${PROJECT_SOURCE_DIR}/src/hb-buffer-serialize.cc
    ${PROJECT_SOURCE_DIR}/src/hb-buffer-verify.cc
    ${PROJECT_SOURCE_DIR}/src/hb-buffer.cc
    ${PROJECT_SOURCE_DIR}/src/hb-common.cc
    ${PROJECT_SOURCE_DIR}/src/hb-coretext.cc
    ${PROJECT_SOURCE_DIR}/src/hb-directwrite.cc
    ${PROJECT_SOURCE_DIR}/src/hb-draw.cc
    ${PROJECT_SOURCE_DIR}/src/hb-face-builder.cc
    ${PROJECT_SOURCE_DIR}/src/hb-face.cc
    ${PROJECT_SOURCE_DIR}/src/hb-fallback-shape.cc
    ${PROJECT_SOURCE_DIR}/src/hb-font.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ft.cc
    ${PROJECT_SOURCE_DIR}/src/hb-gdi.cc
    ${PROJECT_SOURCE_DIR}/src/hb-glib.cc
    ${PROJECT_SOURCE_DIR}/src/hb-graphite2.cc
    ${PROJECT_SOURCE_DIR}/src/hb-map.cc
    ${PROJECT_SOURCE_DIR}/src/hb-number.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-cff1-table.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-cff2-table.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-color.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-face.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-font.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-layout.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-map.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-math.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-meta.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-metrics.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-name.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shape-fallback.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shape-normalize.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shape.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-arabic.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-default.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-hangul.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-hebrew.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-indic-table.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-indic.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-khmer.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-myanmar.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-syllabic.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-thai.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-use.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-shaper-vowel-constraints.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-tag.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ot-var.cc
    ${PROJECT_SOURCE_DIR}/src/hb-outline.cc
    ${PROJECT_SOURCE_DIR}/src/hb-paint-extents.cc
    ${PROJECT_SOURCE_DIR}/src/hb-paint.cc
    ${PROJECT_SOURCE_DIR}/src/hb-set.cc
    ${PROJECT_SOURCE_DIR}/src/hb-shape-plan.cc
    ${PROJECT_SOURCE_DIR}/src/hb-shape.cc
    ${PROJECT_SOURCE_DIR}/src/hb-shaper.cc
    ${PROJECT_SOURCE_DIR}/src/hb-static.cc
    ${PROJECT_SOURCE_DIR}/src/hb-style.cc
    ${PROJECT_SOURCE_DIR}/src/hb-ucd.cc
    ${PROJECT_SOURCE_DIR}/src/hb-unicode.cc
    ${PROJECT_SOURCE_DIR}/src/hb-uniscribe.cc
    ${PROJECT_SOURCE_DIR}/src/hb-wasm-api.cc
    ${PROJECT_SOURCE_DIR}/src/hb-wasm-shape.cc
  )
endif()

set(subset_project_sources
  ${PROJECT_SOURCE_DIR}/src/hb-number.cc
  ${PROJECT_SOURCE_DIR}/src/hb-number.hh
  ${PROJECT_SOURCE_DIR}/src/hb-ot-cff1-table.cc
  ${PROJECT_SOURCE_DIR}/src/hb-ot-cff2-table.cc
  ${PROJECT_SOURCE_DIR}/src/hb-ot-post-table-v2subset.hh
  ${PROJECT_SOURCE_DIR}/src/hb-static.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset-cff-common.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset-cff-common.hh
  ${PROJECT_SOURCE_DIR}/src/hb-subset-cff1.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset-cff2.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset-input.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset-input.hh
  ${PROJECT_SOURCE_DIR}/src/hb-subset-instancer-iup.hh
  ${PROJECT_SOURCE_DIR}/src/hb-subset-instancer-iup.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset-instancer-solver.hh
  ${PROJECT_SOURCE_DIR}/src/hb-subset-instancer-solver.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset-accelerator.hh
  ${PROJECT_SOURCE_DIR}/src/hb-subset-plan.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset-plan.hh
  ${PROJECT_SOURCE_DIR}/src/hb-subset-plan-member-list.hh
  ${PROJECT_SOURCE_DIR}/src/hb-subset-repacker.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset.cc
  ${PROJECT_SOURCE_DIR}/src/hb-subset.hh
  ${PROJECT_SOURCE_DIR}/src/hb-repacker.hh
  ${PROJECT_SOURCE_DIR}/src/graph/graph.hh
  ${PROJECT_SOURCE_DIR}/src/graph/gsubgpos-graph.hh
  ${PROJECT_SOURCE_DIR}/src/graph/gsubgpos-context.hh
  ${PROJECT_SOURCE_DIR}/src/graph/gsubgpos-context.cc
  ${PROJECT_SOURCE_DIR}/src/graph/coverage-graph.hh
  ${PROJECT_SOURCE_DIR}/src/graph/classdef-graph.hh
  ${PROJECT_SOURCE_DIR}/src/graph/pairpos-graph.hh
  ${PROJECT_SOURCE_DIR}/src/graph/markbasepos-graph.hh
  ${PROJECT_SOURCE_DIR}/src/graph/split-helpers.hh
  ${PROJECT_SOURCE_DIR}/src/graph/serialize.hh
  ${PROJECT_SOURCE_DIR}/src/OT/Color/COLR/colrv1-closure.hh
)
set(project_headers
  ${PROJECT_SOURCE_DIR}/src/hb-aat-layout.h
  ${PROJECT_SOURCE_DIR}/src/hb-aat.h
  ${PROJECT_SOURCE_DIR}/src/hb-blob.h
  ${PROJECT_SOURCE_DIR}/src/hb-buffer.h
  ${PROJECT_SOURCE_DIR}/src/hb-common.h
  ${PROJECT_SOURCE_DIR}/src/hb-cplusplus.hh
  ${PROJECT_SOURCE_DIR}/src/hb-deprecated.h
  ${PROJECT_SOURCE_DIR}/src/hb-draw.h
  ${PROJECT_SOURCE_DIR}/src/hb-face.h
  ${PROJECT_SOURCE_DIR}/src/hb-font.h
  ${PROJECT_SOURCE_DIR}/src/hb-map.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-color.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-deprecated.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-font.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-layout.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-math.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-meta.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-metrics.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-name.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-shape.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot-var.h
  ${PROJECT_SOURCE_DIR}/src/hb-ot.h
  ${PROJECT_SOURCE_DIR}/src/hb-paint.h
  ${PROJECT_SOURCE_DIR}/src/hb-set.h
  ${PROJECT_SOURCE_DIR}/src/hb-shape-plan.h
  ${PROJECT_SOURCE_DIR}/src/hb-shape.h
  ${PROJECT_SOURCE_DIR}/src/hb-style.h
  ${PROJECT_SOURCE_DIR}/src/hb-unicode.h
  ${PROJECT_SOURCE_DIR}/src/hb-version.h
  ${PROJECT_SOURCE_DIR}/src/hb.h
)
set(subset_project_headers
  ${PROJECT_SOURCE_DIR}/src/hb-subset.h
  ${PROJECT_SOURCE_DIR}/src/hb-subset-repacker.h
)

list(APPEND THIRD_PARTY_LIBS ${FREETYPE_LIBRARIES})
include_directories(AFTER ${FREETYPE_INCLUDE_DIRS})

list(APPEND project_headers ${PROJECT_SOURCE_DIR}/src/hb-ft.h)

# So check_funcs can find its headers
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${FREETYPE_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${FREETYPE_LIBRARIES})

check_funcs(FT_Get_Var_Blend_Coordinates FT_Set_Var_Blend_Coordinates FT_Done_MM_Var)

# =======================================================
# Harfbuzz library
# =======================================================
add_library(harfbuzz ${project_sources} ${project_headers})
target_link_libraries(harfbuzz ${THIRD_PARTY_LIBS})
target_include_directories(harfbuzz PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
target_link_libraries(harfbuzz freetype)

target_compile_options(harfbuzz PUBLIC /wd4244 /wd4267)
