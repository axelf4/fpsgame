cmake_minimum_required(VERSION 2.8.4)
project(fpsgame)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(PNG REQUIRED)
find_package(FreeType REQUIRED)
find_package(HarfBuzz REQUIRED)

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
	ddsloader.h ddsloader.c
	state.h state.c
	gameState.h gameState.c)

include_directories(
	${objparser_SOURCE_DIR}/include
	${flexLayout_SOURCE_DIR}
	${OPENGL_INCLUDE_DIR}
	${SDL2_INCLUDE_DIR}
	${GLEW_INCLUDE_DIRS}
	${FREETYPE_INCLUDE_DIRS}
	${PNG_INCLUDE_DIRS}
	${HARFBUZZ_INCLUDE_DIRS})
target_link_libraries(fpsgame
	vmath
	objparser
	flexLayout
	${HARFBUZZ_LIBRARIES}
	${FREETYPE_LIBRARIES}
	${GLEW_LIBRARIES}
	${OPENGL_LIBRARIES}
	${SDL2_LIBRARY}
	${PNG_LIBRARIES})
