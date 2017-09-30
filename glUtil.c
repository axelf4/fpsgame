#include "glUtil.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

char *readFile(const char *filename) {
	char *buffer = 0;
	FILE *f = fopen(filename, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		long length = ftell(f);
		rewind(f); // fseek(f, 0, SEEK_SET);
		if ((buffer = malloc(length + 1))) {
			fread(buffer, length, 1, f);
			fclose(f);
			buffer[length] = 0;
		}
	}
	return buffer;
}

void *alignedAlloc(size_t size, size_t align) {
	// return aligned_alloc(align, size);
	// return _mm_malloc(size, align);
	void *mem = malloc(size + 15 + sizeof(void *));
	if (!mem) return 0;
	void *ptr = (void *) (((uintptr_t) mem + 15 + sizeof(void *)) & ~(uintptr_t) 0x0F);
	((void **) ptr)[-1] = mem;
	return ptr;
}

void alignedFree(void *ptr) {
	// free(ptr);
	// _mm_free(ptr);
	if (ptr) free(((void **) ptr)[-1]);
}

float lerp(float a, float b, float f) {
	return a + f * (b - a);
}

GLuint createShader(GLenum type, int count, ...) {
	GLuint shader = glCreateShader(type);
	if (shader == 0) {
		fprintf(stderr, "Error creating shader object.\n");
		return 0;
	}
	va_list sources;
	va_start(sources, count);
	const GLchar *string[count];
	for (int i = 0; i < count; ++i) {
		string[i] = va_arg(sources, const GLchar *);
	}
	va_end(sources);
	glShaderSource(shader, count, string, NULL);
	glCompileShader(shader);

	// Always check the log to not miss warnings
	GLint maxLength;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
	if (maxLength > 0) {
		GLchar infoLog[maxLength];
		glGetShaderInfoLog(shader, maxLength, NULL, infoLog);
		printf("Shader log, type %d: %s\n", type, infoLog);
	}
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status); // Check shader compile status
	if (status == GL_FALSE) {
		glDeleteShader(shader); // Don't leak the shader
		return 0;
	}
	return shader;
}

GLuint createProgram(int count, ...) {
	GLuint program = glCreateProgram();
	if (!program) {
		fprintf(stderr, "Error creating program object.\n");
		return 0;
	}
	va_list args;
	// Attach shaders
	va_start(args, count);
	for (int i = 0; i < count; ++i) {
		GLuint shader = va_arg(args, GLuint);
		(void) va_arg(args, unsigned);
		glAttachShader(program, shader);
	}
	va_end(args);

	glLinkProgram(program); // Link the program

	// Detach shaders
	va_start(args, count);
	for (int i = 0; i < count; ++i) {
		GLuint shader = va_arg(args, GLuint);
		unsigned flags = va_arg(args, unsigned);
		glDetachShader(program, shader);
		if (!(flags & DONT_DELETE_SHADER)) {
			glDeleteShader(shader);
		}
	}
	va_end(args);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint maxLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		GLchar infoLog[maxLength];
		glGetProgramInfoLog(program, maxLength, NULL, infoLog);
		fprintf(stderr, "Program info log: %s\n", infoLog);
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

GLuint createProgramVertFrag(const GLchar *vertexShaderSource, const GLchar *fragmentShaderSource) {
	return createProgram(2, createShader(GL_VERTEX_SHADER, 1, vertexShaderSource), 0,
			createShader(GL_FRAGMENT_SHADER, 1, fragmentShaderSource), 0);
}

float randomFloat() {
	return (float) rand() / RAND_MAX;
}

float cubicBezier(float p0, float p1, float p2, float p3, float t) {
	assert(0.0f <= t && t <= 1.0f && "Input is out of bounds.");
	float s = 1.0f - t;
	return s * s * s * p0 + 3 * s * s * t * p1 + 3 * s * t * t * p2 + t * t * t * p3;
}

void printVector(VECTOR v) {
	ALIGN(16) float vv[4];
	VectorGet(vv, v);
	printf("vector, %f, %f, %f, %f\n", vv[0], vv[1], vv[2], vv[3]);
}

void printMatrix(MATRIX m) {
	ALIGN(16) float mv[16];
	MatrixGet(mv, m);
	printf("%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n",
			mv[0], mv[1], mv[2], mv[3],
			mv[4], mv[5], mv[6], mv[7],
			mv[8], mv[9], mv[10], mv[11],
			mv[12], mv[13], mv[14], mv[15]);
}
