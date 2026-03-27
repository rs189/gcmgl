cmake_minimum_required(VERSION 3.10...3.30)

project(gcmgl CXX)

enable_language(C)

set(CMAKE_BUILD_TYPE Release)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../obj/linux-x86_64)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../obj/linux-x86_64)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin/linux-x86_64)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_SRCS
	src/window/WindowManager.cpp
	src/renderer/Renderer.cpp
	src/renderer/BatchRenderer.cpp
	thirdparty/mathsfury/Maths.cpp
	thirdparty/mathsfury/Vector2.cpp
	thirdparty/mathsfury/Vector3.cpp
	thirdparty/mathsfury/Vector4.cpp
	thirdparty/mathsfury/Matrix4.cpp
	thirdparty/mathsfury/Quaternion.cpp
	src/utils/Utils.cpp
	src/utils/Color.cpp
	src/utils/Time.cpp
)

if(EXAMPLE STREQUAL "Triangle")
	list(APPEND ENGINE_SRCS src/examples/example_triangle/Triangle.cpp)
	add_definitions(-DEXAMPLE_TRIANGLE)
elseif(EXAMPLE STREQUAL "Cube")
	list(APPEND ENGINE_SRCS src/examples/example_cube/Cube.cpp)
	add_definitions(-DEXAMPLE_CUBE)
elseif(EXAMPLE STREQUAL "Batch")
	list(APPEND ENGINE_SRCS src/examples/example_batch/Batch.cpp)
	add_definitions(-DEXAMPLE_BATCH)
elseif(EXAMPLE STREQUAL "Shader")
	list(APPEND ENGINE_SRCS src/examples/example_shader/Shader.cpp)
	add_definitions(-DEXAMPLE_SHADER)
elseif(EXAMPLE STREQUAL "Textured")
	list(APPEND ENGINE_SRCS src/examples/example_textured/Textured.cpp)
	add_definitions(-DEXAMPLE_TEXTURED)
elseif(EXAMPLE STREQUAL "Lit")
	list(APPEND ENGINE_SRCS src/examples/example_lit/Lit.cpp)
	add_definitions(-DEXAMPLE_LIT)
elseif(EXAMPLE STREQUAL "TexturedLit")
	list(APPEND ENGINE_SRCS src/examples/example_texturedLit/TexturedLit.cpp)
	add_definitions(-DEXAMPLE_TEXTURED_LIT)
endif()

if(BUILD_PS3)
	add_definitions(-DPLATFORM_PS3 -mcpu=cell)

	set(CMAKE_SYSTEM_NAME Generic)
	set(CMAKE_C_COMPILER "$ENV{PS3DEV}/bin/ppu-gcc")
	set(CMAKE_CXX_COMPILER "$ENV{PS3DEV}/bin/ppu-g++")
	set(CMAKE_FIND_ROOT_PATH "$ENV{PS3DEV}/ppu" "$ENV{PSL1GHT}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cell -maltivec")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cell -maltivec")

	include_directories(
		$ENV{PSL1GHT}/include
		$ENV{PS3DEV}/ppu/include
		/rsxgl/include
		${CMAKE_SOURCE_DIR}/public
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/thirdparty
	)

	link_directories(
		$ENV{PSL1GHT}/lib
		$ENV{PS3DEV}/ppu/lib
		/rsxgl/lib
	)

	list(APPEND ENGINE_SRCS
		thirdparty/rsxutil/rsxutil.cpp
		src/renderer/gcm/GcmRenderer.cpp
		src/renderer/gcm/GcmBatchRenderer.cpp
		src/renderer/gcm/spu/SpuBatchTransformManager.cpp
		src/window/ps3/WindowPs3.cpp
	)

	add_library(engine STATIC ${ENGINE_SRCS})

	target_include_directories(engine PUBLIC
		${CMAKE_SOURCE_DIR}/include
		${CMAKE_SOURCE_DIR}/include/thirdparty
		/rsxgl/include
	)

	target_link_libraries(engine
		GL
		EGL
		gcm_sys
		rsx
		psl1ght
		lv2
		m
		sysutil
		io
	)

	add_executable(gcmgl src/Main.cpp)
	target_link_libraries(gcmgl engine)

	set_target_properties(gcmgl PROPERTIES
		SUFFIX ".elf"
		LINK_FLAGS "-Wl,-Map=gcmgl.map"
	)

	add_custom_command(TARGET gcmgl POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:gcmgl>/shaders/cg
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_SOURCE_DIR}/shaders/cg $<TARGET_FILE_DIR:gcmgl>/shaders/cg
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:gcmgl>/assets
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:gcmgl>/assets
	)
else()
	find_package(Threads REQUIRED)
	find_package(OpenGL REQUIRED)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GLFW3 glfw3)
	if(NOT GLFW3_FOUND)
		message(FATAL_ERROR "Build failed, glfw3 not found")
	endif()

	add_library(glad STATIC thirdparty/glad/generated/src/gl.c)

	target_include_directories(glad PUBLIC
		${CMAKE_SOURCE_DIR}/thirdparty/glad/generated/include
		${CMAKE_SOURCE_DIR}/thirdparty/glad/include
	)

	file(READ "${CMAKE_SOURCE_DIR}/src/Main.h" MAIN_H_CONTENT)
	if(MAIN_H_CONTENT MATCHES "(^|\n)[ \t]*#[ \t]*define[ \t]+GCMGL_DIAGNOSTICS")
		list(APPEND ENGINE_SRCS src/utils/DiagnosticsReporter.cpp)
	endif()

	list(APPEND ENGINE_SRCS
		src/renderer/gl/GlRenderer.cpp
		src/renderer/gl/GlBatchRenderer.cpp
		src/window/glfw/WindowGlfw.cpp
	)

	add_library(engine STATIC ${ENGINE_SRCS})

	target_include_directories(engine PUBLIC
		${CMAKE_SOURCE_DIR}/public
		${CMAKE_SOURCE_DIR}/include
		${CMAKE_SOURCE_DIR}/include/thirdparty
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/thirdparty
		${CMAKE_SOURCE_DIR}/thirdparty/simde
		${OPENGL_INCLUDE_DIR}
		${GLFW3_INCLUDE_DIRS}
	)

	target_link_libraries(engine PUBLIC
		Threads::Threads
		OpenGL::GL
		${GLFW3_LIBRARIES}
		glad
	)

	target_link_directories(engine PUBLIC ${GLFW3_LIBRARY_DIRS})

	add_executable(gcmgl src/Main.cpp)
	target_link_libraries(gcmgl engine)

	add_custom_command(TARGET gcmgl POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:gcmgl>/shaders/glsl
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_SOURCE_DIR}/shaders/glsl $<TARGET_FILE_DIR:gcmgl>/shaders/glsl
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:gcmgl>/assets
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:gcmgl>/assets
	)
endif()