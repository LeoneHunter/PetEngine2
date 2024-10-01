include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#=======================================================
# Directx headers
#=======================================================
add_subdirectory(d3d12_headers)

add_library(dx_headers INTERFACE)
target_link_libraries(dx_headers INTERFACE 
	Microsoft::DirectX-Headers 
	Microsoft::DirectX-Guids
)
add_library(thirdparty::dx_headers ALIAS dx_headers)



#=======================================================
# ImGui
#=======================================================
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
add_subdirectory(freetype)
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
add_library(doctest INTERFACE)
add_library(thirdparty::doctest ALIAS doctest)
target_sources(doctest INTERFACE doctest/doctest.h)
target_include_directories(doctest 
  	INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/doctest>"
)