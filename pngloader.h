#ifndef PNGLOADER_H
#define PNGLOADER_H

#include <GL/glew.h>

unsigned char *loadPngData(const char *filename, int *width, int *height, GLenum *format);

/**
 * Loads an OpenGL texture from image data.
 * @return The texture id or 0 if an error occurred.
 */
GLuint loadPngTextureFromData(unsigned char *data, int width, int height, GLenum format);

GLuint loadPngTexture(const char *filename, int *width, int *height);

#endif
