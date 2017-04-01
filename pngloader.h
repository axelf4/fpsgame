#ifndef PNGLOADER_H
#define PNGLOADER_H

#include <png.h>
#include <GL/glew.h>

GLuint loadPNGTexture(const char *filename, png_uint_32 *width, png_uint_32 *height);

#endif
