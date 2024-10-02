include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(FetchContent)


#=======================================================
# Directx headers
#=======================================================
FetchContent_Declare(
	d3d12_headers
	GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/d3d12_headers"
	GIT_TAG origin/main
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(d3d12_headers)

add_library(dx_headers INTERFACE)
target_link_libraries(dx_headers INTERFACE 
	DirectX-Headers
	Microsoft::DirectX-Headers 
	Microsoft::DirectX-Guids
)
add_library(thirdparty::dx_headers ALIAS dx_headers)


#=======================================================
# ImGui
#=======================================================
FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
	GIT_TAG v1.88
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(ImGui STATIC)
target_include_directories(ImGui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui>
	# For imconfig.h including math_util.h for vec conversions
	${CMAKE_SOURCE_DIR}
)
target_sources(ImGui PUBLIC 
	imgui/imgui.cpp 
	imgui/imgui_draw.cpp 
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
)
add_library(thirdparty::imgui ALIAS ImGui)


#=======================================================
# FreeType (HarfBuzz depends on this)
#=======================================================
FetchContent_Declare(
	freetype
	GIT_REPOSITORY https://github.com/freetype/freetype.git
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/freetype"
	GIT_TAG VER-2-13-3
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freetype)
add_library(thirdparty::freetype ALIAS freetype)


#=======================================================
# HarfBuzz
#=======================================================
add_subdirectory(harfbuzz)
add_library(HarfBuzz::HarfBuzz ALIAS harfbuzz)
add_library(thirdparty::harfbuzz ALIAS harfbuzz)


#=======================================================
# Doctest
#=======================================================
FetchContent_Declare(
	doctest
	GIT_REPOSITORY https://github.com/doctest/doctest.git
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doctest"
	GIT_TAG v2.4.11
	GIT_SHALLOW TRUE
)

FetchContent_GetProperties(doctest)
if(NOT doctest_POPULATED)
  FetchContent_Populate(doctest)
endif()

add_library(doctest INTERFACE)
add_library(thirdparty::doctest ALIAS doctest)
target_sources(doctest INTERFACE doctest/doctest.h)
target_include_directories(doctest 
  	INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/doctest>"
)