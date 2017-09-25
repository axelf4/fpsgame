#ifndef NINE_PATCH_H
#define NINE_PATCH_H

#include <GL/glew.h>
#include "widget.h"

struct NinePatch {
	struct Widget widget;
	GLuint texture;
	int width, height;
	float t, r, b, l;
	float padTop, padRight, padBottom, padLeft;
	/** Optional child. */
	struct Widget *child;
};

void ninePatchInit(struct Widget *widget, GLuint texture, int width, int height, float t, float r, float b, float l);

#endif
