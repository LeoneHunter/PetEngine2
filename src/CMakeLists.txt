include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(
  GLOB_RECURSE
  SRC_FILES
  editor/*.cpp
  runtime/*.cpp
  thirdparty/*.cpp
  main.cpp)

file(
  GLOB_RECURSE
  H_FILES
  editor/*.h
  runtime/*.h
  thirdparty/*.h)

set(FILES ${SRC_FILES} ${H_FILES})

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/optik/lib/x64/release/OptickCore.lib)
target_link_libraries(${PROJECT_NAME} PRIVATE "d3d12.lib" "dxgi.lib" "d3dcompiler.lib")

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

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|x86_64|AMD64|amd64|i386|i686")

	set(HAVE_NEON FALSE)
	set(HAVE_AVX ON)
	set(AVX_COMPILE_FLAGS "/arch:AVX")
	add_definitions("-DHAVE_AVX")

	set(HAVE_AVX2 ON)
	set(AVX2_COMPILE_FLAGS "/arch:AVX2")
	add_definitions("-DHAVE_AVX2")

	set(HAVE_AVX512F ON)
	set(AVX512F_COMPILE_FLAGS "/arch:AVX512")
	add_definitions("-DHAVE_AVX512F")

	set(HAVE_FMA ON)
	set(FMA_COMPILE_FLAGS "-D__FMA__")
	add_definitions("-DHAVE_FMA")

	set(HAVE_SSE4_1 ON)
	set(SSE4_1_COMPILE_FLAGS "-D__SSE4_1__")
	add_definitions("-DHAVE_SSE4_1")

endif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|x86_64|AMD64|amd64|i386|i686")