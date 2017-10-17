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

/**
 * Loads an OpenGL cubemap texture from PNG files containing the 6 faces.
 * @param files The filenames of the six cubemap faces.
 * @return Returns the texture id or \c 0 in case of an error.
 */
GLuint loadCubemapFromPng(const char *files[static 6]);

#endif
