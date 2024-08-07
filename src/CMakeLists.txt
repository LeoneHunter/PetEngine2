
# Include thirdparty targets
add_subdirectory(thirdparty)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})



#=======================================================
# Base
#=======================================================
add_library(Base)

set(BASE_FILES 
	# UI
	editor/ui/button.h
	editor/ui/containers.h
	editor/ui/containers.cpp
	editor/ui/focus.h
	editor/ui/font.h
	editor/ui/font.cpp
	editor/ui/framework.h
	editor/ui/framework.cpp
	editor/ui/popup.h
	editor/ui/scroll_view.h
	editor/ui/style.h
	editor/ui/tooltip.h
	editor/ui/ui.h
	editor/ui/widgets.h
	editor/ui/widgets.cpp
	# Runtime
	runtime/common.h
	runtime/error.h
	runtime/image.h
	runtime/input_types.h
	runtime/log.h
	runtime/log.cpp
	runtime/math_util.h
	runtime/native_window.h
	runtime/native_window.cpp
	runtime/rtti.h
	runtime/string_utils.h
	runtime/string_utils.cpp
	runtime/threading.h
	runtime/threading.cpp
	runtime/ui_renderer.h
	runtime/ui_renderer.cpp
	runtime/util.h
	runtime/win_minimal.h
	runtime/win_minimal.cpp
	# Task
	runtime/task/future.h
	runtime/task/task_executor.h
	runtime/task/task_executor.cpp
	runtime/task/task_source.h
	runtime/task/task_source.cpp
	runtime/task/task_tracker.h
	runtime/task/task_tracker.cpp
	runtime/task/task.h
	# Rendering
	runtime/renderer/d3d12/common_dx12.h
	runtime/renderer/d3d12/device_dx12.h
	runtime/renderer/d3d12/device_dx12.cpp
	runtime/renderer/d3d12/allocator_dx12.h
	runtime/renderer/d3d12/allocator_dx12_heap.h
	runtime/renderer/d3d12/descriptor_heap_dx12.h
	runtime/renderer/d3d12/descriptor_heap_dx12.cpp
)

target_sources(Base PRIVATE ${BASE_FILES})
target_link_libraries(Base PUBLIC
	d3d12.lib 
	dxgi.lib 
	d3dcompiler.lib
	dxguid.lib
	# In local /thirdparty folder
	thirdparty::optick
	thirdparty::dx_headers
	thirdparty::imgui
)
target_compile_definitions(Base PUBLIC
	WIN32_LEAN_AND_MEAN
	NOMINMAX
	UNICODE
	_UNICODE
)
target_link_options(Base PUBLIC
	$<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>
	# suppress linking warning due to /INCREMENTAL and /OPT:ICF being both ON
	$<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
	# Ignore .pdb missing for third party libs
	/ignore:4099
	# vcpkg freetype has been compiled with libcmt
	/NODEFAULTLIB:libcmt.lib
)



#=======================================================
# App test executable
#=======================================================
# Exe name in the form "project_name_configuration.exe"
add_executable(UI_Test_Filesystem_View)
target_sources(UI_Test_Filesystem_View PRIVATE ui_test_filesystem_view.cpp)
target_link_libraries(UI_Test_Filesystem_View PRIVATE Base)
set_target_properties(UI_Test_Filesystem_View PROPERTIES OUTPUT_NAME UI_Test_Filesystem_View_$<CONFIG>)

set_target_properties(UI_Test_Filesystem_View PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY 
	${CMAKE_SOURCE_DIR}/bin
)
set_target_properties(UI_Test_Filesystem_View PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY_RELEASE 
	${CMAKE_SOURCE_DIR}/bin
)
set_target_properties(UI_Test_Filesystem_View PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY_DEBUG 
	${CMAKE_SOURCE_DIR}/bin
)
set_target_properties(UI_Test_Filesystem_View PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO 
	${CMAKE_SOURCE_DIR}/bin
)



#=======================================================
# Tests.exe 
#=======================================================
add_executable(Tests)

file(GLOB_RECURSE FOUND_FILES
	editor/*.cpp
	editor/*.h
	runtime/*.cpp
	runtime/*.h
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
target_link_libraries(Tests PRIVATE Base)

set_target_properties(Tests PROPERTIES 
OUTPUT_NAME 
Tests_$<CONFIG>
)
set_target_properties(Tests PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY 
${CMAKE_SOURCE_DIR}/bin
)
set_target_properties(Tests PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY_RELEASE 
	${CMAKE_SOURCE_DIR}/bin
)
set_target_properties(Tests PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY_DEBUG 
	${CMAKE_SOURCE_DIR}/bin
)
set_target_properties(Tests PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO 
	${CMAKE_SOURCE_DIR}/bin
)

add_test(NAME UnitTests COMMAND Tests)