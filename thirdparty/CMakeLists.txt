include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(FetchContent)

# =======================================================
# Directx headers
# =======================================================
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

# =======================================================
# ImGui
# =======================================================
add_subdirectory(imgui)
add_library(thirdparty::imgui ALIAS imgui)


# =======================================================
# ZLib
# =======================================================
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable tests")
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Disable shared")
set(ZLIB_INSTALL OFF CACHE BOOL "Disable install")

FetchContent_Declare(
	zlib
	GIT_REPOSITORY https://github.com/madler/zlib
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zlib"
	BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/zlib"
	GIT_TAG v1.3.1
	GIT_SHALLOW TRUE
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(zlib)

add_library(ZLIB::ZLIB ALIAS zlibstatic)
add_library(thirdparty::zlib ALIAS zlibstatic)



# =======================================================
# FreeType 
# =======================================================
set(FT_DISABLE_ZLIB ON CACHE BOOL "Use internal zlib")

FetchContent_Declare(
	freetype
	GIT_REPOSITORY https://github.com/freetype/freetype.git
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/freetype"
	GIT_TAG VER-2-13-3
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freetype)
add_library(thirdparty::freetype ALIAS freetype)

# =======================================================
# HarfBuzz
# =======================================================
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "No dev warnings")
add_subdirectory(harfbuzz)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 0 CACHE INTERNAL "Re-enable dev warnings")

add_library(HarfBuzz::HarfBuzz ALIAS harfbuzz)
add_library(thirdparty::harfbuzz ALIAS harfbuzz)

# =======================================================
# Doctest
# =======================================================
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "No dev warnings")
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

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 0 CACHE INTERNAL "Re-enable dev warnings")

add_library(doctest INTERFACE)
add_library(thirdparty::doctest ALIAS doctest)
target_sources(doctest INTERFACE doctest/doctest.h)
target_include_directories(doctest
	INTERFACE
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/doctest>"
)

# =======================================================
# LibPNG
# =======================================================
set(PNG_SHARED OFF CACHE BOOL "Disable shared lib")
set(PNG_TESTS OFF CACHE BOOL "Disable tests")
set(PNG_TOOLS OFF CACHE BOOL "Disable tools")
set(SKIP_INSTALL_ALL ON CACHE BOOL "Disable libpng install")

FetchContent_Declare(
	libpng
	GIT_REPOSITORY https://github.com/pnggroup/libpng
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libpng"
	GIT_TAG v1.6.48
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libpng)

add_library(thirdparty::libpng ALIAS png_static)

