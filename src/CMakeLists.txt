
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
        ui_filesystem_sample
    SRCS
        ui_test_filesystem_view.cpp
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
# Tests.exe 
#=======================================================
add_executable(Tests)

file(GLOB_RECURSE FOUND_FILES
    ./*.cpp
    ./*.h
)

# Files with "_test.*"
foreach(FILE ${FOUND_FILES})
    get_filename_component(FILENAME ${FILE} NAME_WLE)
    string(FIND ${FILENAME} "_test" IS_TEST_FILE)

    if(NOT ${IS_TEST_FILE} EQUAL -1)
        set(TEST_FILES ${TEST_FILES} ${FILE})
    endif()
endforeach()

set(FILES ${TEST_FILES} test_main.cpp)
target_sources(Tests PRIVATE ${FILES})
target_link_libraries(Tests 
    PRIVATE 
    base 
    thirdparty::doctest
)

set_target_properties(Tests PROPERTIES 
    OUTPUT_NAME 
    Tests_$<CONFIG>
)
set_target_properties(Tests PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY 
    ${CMAKE_SOURCE_DIR}/bin
)

add_test(NAME UnitTests COMMAND Tests)