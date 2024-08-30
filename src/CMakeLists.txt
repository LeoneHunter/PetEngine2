
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(base)
add_subdirectory(gfx)
add_subdirectory(gfx_legacy)
add_subdirectory(gpu)
add_subdirectory(task)
add_subdirectory(ui)


#=======================================================
# Sample app testing simple ui
# Shows a directory tree of a test folder
#=======================================================
executable(
    NAME
        filesystem_ui_sample
    SRCS
        filesystem_ui_sample.cpp
    DEPS
        base
        ui
        gfx_legacy
        thirdparty::dx_headers
        thirdparty::imgui
)


#=======================================================
# Freetype Example cli
#=======================================================
# Exe name in the form "project_name_configuration.exe"
executable(
    NAME
        freetype_sample
    SRCS
        freetype_sample.cpp
    DEPS
        base
        thirdparty::freetype
        thirdparty::harfbuzz
    COPTS
        /utf8
)


#=======================================================
# Canvas sample
# Shows a rectange on a window using gfx::Canvas
#=======================================================
executable(
    NAME
        canvas_sample
    SRCS
        canvas_sample.cpp
    DEPS
        base 
        gfx
        gfx_legacy
        gpu
)

#=======================================================
# GPU sample
# Tests gpu module
#=======================================================
executable(
    NAME
        hello_triangle
    SRCS
        hello_triangle.cpp
    DEPS
        base 
        gpu
)


#=======================================================
# Buddy alloc
#=======================================================
executable(
    NAME
        buddy_alloc
    SRCS
        buddy_alloc.cpp
    DEPS
        base 
)
