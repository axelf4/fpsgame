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

// TODO make width and height nullable
GLuint loadPNGTexture(const char *filename, png_uint_32 *width, png_uint_32 *height) {
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
	int bit_depth, color_type;
	png_get_IHDR(png_ptr, info_ptr, width, height, &bit_depth, &color_type, NULL, NULL, NULL);
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
	GLint format = getGLColorFormat(color_type);

	png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	rowbytes += 3 - ((rowbytes - 1) % 4); // glTexImage2d requires rows to be 4-byte aligned
	png_byte *imageData = malloc(sizeof(png_byte) * rowbytes * *height);
	if (!imageData) {
		fprintf(stderr, "error: could not allocate memory for png_ptr image data.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		return 0;
	}
	png_bytepp rowPointers = malloc(sizeof(png_bytep) * *height);
	if (!rowPointers) {
		fprintf(stderr, "error: could not allocate memory for row pointers.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(imageData);
		fclose(fp);
		return 0;
	}
	// Point the row pointers at the correct offsets of imageData
	for (unsigned int i = 0; i < *height; ++i) {
		rowPointers[i] = imageData + i * rowbytes;
	}
	// Read the image data through rowPointers
	png_read_image(png_ptr, rowPointers);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, format, *width, *height, 0, format, GL_UNSIGNED_BYTE, imageData);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// clean up
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(imageData);
	free(rowPointers);
	fclose(fp);
	return texture;
}
