#include "pngloader.h"
#include <stdlib.h>
#include <stdio.h>
#include <png.h>

static GLenum getGLColorFormat(const int color_type) {
	switch (color_type) {
		case PNG_COLOR_TYPE_RGB:
			return GL_RGB;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			return GL_RGBA;
		case PNG_COLOR_TYPE_GRAY:
			return GL_LUMINANCE;
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			return GL_LUMINANCE_ALPHA;
		default:
			fprintf(stderr, "Unknown libpng color type %d.\n", color_type);
			return 0;
	}
}

unsigned char *loadPngData(const char *filename, int *width, int *height, GLenum *format) {
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		perror(filename);
		return 0;
	}
	// Create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fprintf(stderr, "Error: png_create_read_struct returned 0.\n");
		fclose(fp);
		return 0;
	}
	// Create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		fprintf(stderr, "Error: png_create_info_struct returned 0.\n");
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(fp);
		return 0;
	}
	// Set up error handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "Error from libPNG when reading png_ptr file.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		return 0;
	}
	// Set up PNG reading
	png_init_io(png_ptr, fp);
	png_read_info(png_ptr, info_ptr); // Read information up to the image data
	png_uint_32 imageWidth, imageHeight;
	int bit_depth, color_type;
	png_get_IHDR(png_ptr, info_ptr, &imageWidth, &imageHeight, &bit_depth, &color_type, NULL, NULL, NULL);
	if (width) *width = imageWidth;
	if (height) *height = imageHeight;
	// Convert transparency to full alpha
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png_ptr);
	}
	// Convert grayscale
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}
	// Convert paletted images
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
	}
	// Add alpha channel (GL_RGBA is faster than GL_RGB on many GPUs)
	if (color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_RGB) {
		png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}
	// Ensure 8-bit packing
	if (bit_depth < 8) {
		png_set_packing(png_ptr);
	} else if (bit_depth == 16) {
		png_set_scale_16(png_ptr);
	}
	png_read_update_info(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	if (format) *format = getGLColorFormat(color_type);

	png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	rowbytes += 3 - ((rowbytes - 1) % 4); // glTexImage2d requires rows to be 4-byte aligned
	png_byte *imageData = malloc(sizeof(png_byte) * rowbytes * imageHeight);
	if (!imageData) {
		fprintf(stderr, "error: could not allocate memory for png_ptr image data.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		return 0;
	}
	png_bytepp rowPointers = malloc(sizeof(png_bytep) * imageHeight);
	if (!rowPointers) {
		fprintf(stderr, "error: could not allocate memory for row pointers.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(imageData);
		fclose(fp);
		return 0;
	}
	// Point the row pointers at the correct offsets of imageData
	for (unsigned int i = 0; i < imageHeight; ++i) {
		rowPointers[i] = imageData + i * rowbytes;
	}
	png_read_image(png_ptr, rowPointers); // Read the image data through rowPointers

	free(rowPointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(fp);
	return imageData;
}

GLuint loadPngTextureFromData(unsigned char *data, int width, int height, GLenum format) {
	GLuint texture;
	glGenTextures(1, &texture);
	if (!texture) {
		fprintf(stderr, "Failed to create texture.\n");
		return 0;
	}
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return texture;
}

GLuint loadPngTexture(const char *filename, int *width, int *height) {
	GLenum format;
	int imageWidth, imageHeight;
	unsigned char *data = loadPngData(filename, &imageWidth, &imageHeight, &format);
	if (!data) {
		fprintf(stderr, "Failed to load PNG data.\n");
		return 0;
	}
	GLuint texture = loadPngTextureFromData(data, imageWidth, imageHeight, format);
	free(data);
	if (!texture) {
		return 0;
	}
	if (width) *width = imageWidth;
	if (height) *height = imageHeight;
	return texture;
}

/**
 * Loads an OpenGL cubemap texture from files containing the 6 faces.
 * @return Returns the texture id or \c 0 in case of an error.
 */
GLuint loadCubemapFromPng(char *files[static 6]) {
	GLuint texture;
	glGenTextures(1, &texture);
	if (!texture) {
		fprintf(stderr, "Failed to create OpenGL texture.\n");
		return 0;
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

	for (int i = 0; i < 6; ++i) {
	}

	return texture;
}
