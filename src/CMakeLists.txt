include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME})

set(FILES ${MAIN_FILES} main.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/optik/lib/x64/release/OptickCore.lib)
target_link_libraries(${PROJECT_NAME} PRIVATE "d3d12.lib" "dxgi.lib" "d3dcompiler.lib")

# target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/freetype/lib/freetype.lib)
# target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/freetype/include)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin")
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin")
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin")

target_compile_definitions(${PROJECT_NAME} PRIVATE
	WIN32_LEAN_AND_MEAN
	NOMINMAX
	UNICODE
	_UNICODE
)

target_link_options(${PROJECT_NAME} PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>
	# suppress linking warning due to /INCREMENTAL and /OPT:ICF being both ON
	$<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
)


