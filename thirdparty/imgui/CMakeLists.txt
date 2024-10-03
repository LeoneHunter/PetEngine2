
FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
	GIT_TAG v1.88
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# Copy our config into the project for convenience
file(
	COPY ${CMAKE_CURRENT_SOURCE_DIR}/imconfig.h 
    DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)

add_library(imgui STATIC)
target_include_directories(imgui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        # For imconfig.h including math_util.h for vec conversions
        ${CMAKE_SOURCE_DIR}
)

target_sources(imgui PUBLIC 
	imgui/imgui.cpp 
	imgui/imgui_draw.cpp 
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
)