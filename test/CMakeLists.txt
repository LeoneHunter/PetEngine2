include_directories("../src")

add_executable(Tests)

set(FILES ${MAIN_FILES} ${TEST_FILES} test.cpp)

target_sources(Tests PRIVATE ${FILES})

target_link_libraries(Tests PRIVATE ${CMAKE_SOURCE_DIR}/src/thirdparty/optik/lib/x64/release/OptickCore.lib)
target_link_libraries(Tests PRIVATE "d3d12.lib" "dxgi.lib" "d3dcompiler.lib")

target_compile_definitions(Tests PRIVATE
	WIN32_LEAN_AND_MEAN
	NOMINMAX
	UNICODE
	_UNICODE
)

target_compile_options(Tests PRIVATE
	/W3
)

target_link_options(Tests PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>
	# suppress linking warning due to /INCREMENTAL and /OPT:ICF being both ON
	$<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
)

target_compile_features(Tests PRIVATE cxx_std_23)

set_target_properties(Tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/test/bin")
set_target_properties(Tests PROPERTIES OUTPUT_NAME Test)
set_target_properties(Tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/test/bin")
set_target_properties(Tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/test/bin")
set_target_properties(Tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/test/bin")

add_test(NAME UnitTests COMMAND Tests)