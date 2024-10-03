
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

# Create a testing directory tree
add_custom_command(
    TARGET filesystem_ui_sample
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory 
        "${OUTPUT_DIRECTORY}/test_dir_tree"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child1"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child4"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child5"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child6"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child7"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child8"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child9"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child10"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child11"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child12"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child13"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child1/child14"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child2"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child4"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child5"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child6"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child7"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child8"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child9"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child10"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child11"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child12"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child13"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child2/child14"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child3"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child3/child1"
            "${OUTPUT_DIRECTORY}/test_dir_tree/child3/child2"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child4"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child5"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child6"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child7"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child8"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child9"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child10"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child11"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child12"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child13"
        "${OUTPUT_DIRECTORY}/test_dir_tree/child14"
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
        thirdparty::imgui
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
