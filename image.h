#ifndef IMAGE_H
#define IMAGE_H

#include "widget.h"
#include <gl/glew.h>

enum {
	ALIGN_TOP,
	ALIGN_RIGHT,
	ALIGN_BOTTOM,
	ALIGN_LEFT
};

struct Image {
	struct Widget widget;
	GLuint texture;

	float imageX, imageY;
	float regionX, regionY, regionWidth, regionHeight;
	float textureWidth, textureHeight;

	int align;
};

void imageInitialize(struct Widget *widget, GLuint texture, float width, float height, int align);

#endif
