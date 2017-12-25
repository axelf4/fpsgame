#ifndef GL_UTIL_H
#define GL_UTIL_H

#include <stddef.h>
#include <GL/glew.h>
#include <vmath.h>

/** Clamps a value between a minimum and maximum value. */
#define CLAMP(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

enum {
	DONT_DELETE_SHADER = 0x1
};

/**
 * Returned pointer has to be freed with free.
 */
char *readFile(const char *filename);

void *alignedAlloc(size_t size, size_t align);
void alignedFree(void *ptr);

float lerp(float a, float b, float f);

/**
 * Creates a shader object from the specified source strings.
 */
GLuint createShader(GLenum type, int count, ...);

/**
 * Creates and links a program.
 * Takes in pairs of shader objects and unsigned shader flags.
 */
GLuint createProgram(int count, ...);

/**
 * Creates and links a shader program with from the specified shader sources.
 */
GLuint createProgramVertFrag(const GLchar *vertexShaderSource, const GLchar *fragmentShaderSource);

/**
 * Returns a random float between 0 and 1.
 */
float randomFloat();

float cubicBezier(float p0, float p1, float p2, float p3, float t);

int isSphereCollision(VECTOR pos0, VECTOR pos1, float radius0, float radius1, VECTOR movevec);

void printVector(VECTOR v);

void printMatrix(MATRIX m);

#endif
