#ifndef GL_UTIL_H
#define GL_UTIL_H

#include <GL/glew.h>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

/**
 * Returned pointer has to be freed with free.
 */
char *readFile(const char *filename);

/*
 * Creates a shader program and attaches the shaders but DOES NOT link the program.
 */
GLuint createProgram(const GLchar *vertexShaderSource, const GLchar *fragmentShaderSource);

#endif
