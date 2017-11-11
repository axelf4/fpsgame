cmake_minimum_required(VERSION 2.8.4)
project(fpsgame)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	find_package(OpenGL REQUIRED)
	find_package(SDL2 REQUIRED)
	find_package(GLEW REQUIRED)
	find_package(PNG REQUIRED)
	find_package(Freetype REQUIRED)
	find_package(HarfBuzz REQUIRED)
endif()

add_subdirectory(lib/vmath)
add_subdirectory(lib/objparser)
add_subdirectory(lib/flexLayout)

add_executable(fpsgame
	main.c
	pngloader.h pngloader.c
	model.h model.c
	stb_rect_pack.h
	glUtil.h glUtil.c
	font.h font.c
	spriteBatch.h spriteBatch.c
	linebreak.h linebreak.c
	widget.h widget.c
	label.h label.c
	image.h image.c
	state.h state.c
	gameState.h gameState.c
	renderer.h renderer.c
	entity.h entity.c
	box.h box.c
	button.h button.c
	ninePatch.h ninePatch.c)

include_directories(
	${objparser_SOURCE_DIR}/include
	lib/harfbuzz/src)
target_link_libraries(fpsgame
	vmath
	objparser
	flexLayout)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	include_directories(
		${OPENGL_INCLUDE_DIR}
		${SDL2_INCLUDE_DIR}
		${GLEW_INCLUDE_DIRS}
		${FREETYPE_INCLUDE_DIRS}
		${PNG_INCLUDE_DIRS}
		${HARFBUZZ_INCLUDE_DIRS})
	target_link_libraries(fpsgame
		${HARFBUZZ_LIBRARIES}
		${FREETYPE_LIBRARIES}
		${GLEW_LIBRARIES}
		${OPENGL_LIBRARIES}
		${SDL2_LIBRARY}
		${PNG_LIBRARIES}
		m)
endif()
