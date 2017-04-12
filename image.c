#include "image.h"
#include <stdio.h>
#include <assert.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static float resolveAdjustedSize(float desiredSize, float size, enum MeasureMode mode) {
	switch (mode) {
		case MEASURE_UNSPECIFIED:
			return desiredSize;
		case MEASURE_AT_MOST:
			return MIN(desiredSize, size);
		case MEASURE_EXACTLY:
			return size;
		default:
			assert(0);
	}
}

static void imageLayout(struct Widget *widget, float widthSize, MeasureMode widthMode, float heightSize, MeasureMode heightMode) {
	struct Image *image = (struct Image *) widget;
	float width, height;
	float w = image->regionWidth, h = image->regionHeight;
	const float desiredAspect = image->regionWidth / image->regionHeight;

	int resizeWidth = widthMode != MEASURE_EXACTLY, resizeHeight = heightMode != MEASURE_EXACTLY;
	if (resizeWidth || resizeHeight) {
		width = resolveAdjustedSize(w, widthSize, widthMode);
		height = resolveAdjustedSize(h, heightSize, heightMode);

		float actualAspect = widthSize / heightSize;
		if (desiredAspect != actualAspect) {
			int done = 0;
			if (resizeWidth) {
				float newWidth = desiredAspect * heightSize;
				if (!resizeHeight) {
					widthSize = resolveAdjustedSize(newWidth, widthSize, widthMode);
				}
				if (newWidth <= width) {
					width = newWidth;
					done = 1;
				}
			}
			if (!done && resizeHeight) {
				float newHeight = width / desiredAspect;
				if (!resizeWidth) {
					heightSize = resolveAdjustedSize(newHeight, heightSize, heightMode);
				}
				if (newHeight <= height) {
					height = newHeight;
				}
			}
		}
	} else {
		width = resolveAdjustedSize(w, widthSize, widthMode);
		height = resolveAdjustedSize(h, heightSize, heightMode);
	}
	widget->width = width;
	widget->height = height;

	if (image->align & ALIGN_LEFT) {
		image->imageX = 0;
	} else if (image->align & ALIGN_RIGHT) {
		image->imageX = width - w;
	} else {
		image->imageX = (width - w) / 2;
	}

	if (image->align & ALIGN_TOP) {
		image->imageY = 0;
	} else if (image->align & ALIGN_BOTTOM) {
		image->imageY = height - h;
	} else {
		image->imageY = (height - h) / 2;
	}

	widgetMarkValidated(widget);
}

static void imageDraw(struct Widget *widget, struct SpriteBatch *renderer) {
	struct Image *image = (struct Image *) widget;
	image->imageX = 0;
	image->imageY = 0;
	spriteBatchDraw(renderer, image->texture,
			widget->x + image->imageX, widget->y + image->imageY,
			widget->width, widget->height);
}

static struct WidgetClass imageClass = {
	imageLayout, imageDraw
};

void imageInitialize(struct Widget *widget, GLuint texture, float width, float height, int align) {
	widgetInitialize(widget);
	struct Image *image = (struct Image *) widget;
	image->widget.vtable = &imageClass;
	image->texture = texture;
	image->align = align;

	image->imageX = image->imageY = 0;
	image->regionX = image->regionY = 0;
	image->regionWidth = image->textureWidth = width;
	image->regionHeight = image->textureHeight = height;
}
