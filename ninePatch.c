#include "ninePatch.h"
#include <stdio.h>
#include <assert.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static void ninePatchLayout(struct Widget *widget, float width, enum MeasureMode widthMode, float height, enum MeasureMode heightMode) {
	struct NinePatch *ninePatch = (struct NinePatch *) widget;
	struct Widget *child = ninePatch->child;

	if (child) {
		child->x = ninePatch->padLeft;
		child->y = ninePatch->padTop;
		float childWidth = width - ninePatch->padLeft - ninePatch->padRight,
			  childHeight = height - ninePatch->padTop - ninePatch->padBottom;
		widgetLayout(child, childWidth, widthMode, childHeight, heightMode);
		width = child->width + ninePatch->padLeft + ninePatch->padRight;
		height = child->height + ninePatch->padTop + ninePatch->padBottom;
	} else {
		if (widthMode == MEASURE_UNSPECIFIED) width = ninePatch->l + ninePatch->r;
		if (heightMode == MEASURE_UNSPECIFIED) height = ninePatch->t + ninePatch->b;
	}

	widget->width = width;
	widget->height = height;
}

static void ninePatchDraw(struct Widget *widget, struct SpriteBatch *batch) {
	struct NinePatch *ninePatch = (struct NinePatch *) widget;
	GLuint texture = ninePatch->texture;
	float x = widget->x, y = widget->y, width = widget->width, height = widget->height;
	float t = ninePatch->t, r = ninePatch->r, b = ninePatch->b, l = ninePatch->l;
	float sw = ninePatch->width, sh = ninePatch->height;

	spriteBatchDrawCustom(batch, texture, x, y, x + l, y + t, 0, 0, l / sw, t / sh, white); // Top left
	spriteBatchDrawCustom(batch, texture, x + l, y, x + width - r, y + t, l / sw, 0, (sw - r) / sw, t / sh, white); // Top middle
	spriteBatchDrawCustom(batch, texture, x + width - r, y, x + width, y + t, (sw - r) / sw, 0, 1,  t / sh, white); // To pright

	spriteBatchDrawCustom(batch, texture, x, y + t, x + l, y + height - b, 0,  t / sh,  l / sw,  (sh - b) / sh, white); // Top left
	spriteBatchDrawCustom(batch, texture, x + l, y + t, x + width - r, y + height - b,  l / sw,  t / sh,  (sw - r) / sw,  (sh - b) / sh, white); // Top middle
	spriteBatchDrawCustom(batch, texture, x + width - r, y + t, x + width, y + height - b,  (sw - r) / sw,  t / sh, 1,  (sh - b) / sh, white); // To pright

	spriteBatchDrawCustom(batch, texture, x, y + height - b, x + l, y + height, 0, (sh - b) / sh, l / sw, 1, white); // Top left
	spriteBatchDrawCustom(batch, texture, x + l, y + height - b, x + width - r, y + height, l / sw, (sh - b) / sh, (sw - r) / sw, 1, white); // Top middle
	spriteBatchDrawCustom(batch, texture, x + width - r, y + height - b, x + width, y + height, (sw - r) / sw, (sh - b) / sh, 1,  1, white); // To pright

	struct Widget *child = ninePatch->child;
	if (child) {
		child->x += x;
		child->y += y;
		widgetDraw(child, batch);
		child->x -= x;
		child->y -= y;
	}
}

static struct WidgetClass ninePatchClass = {
	ninePatchLayout, ninePatchDraw
};

void ninePatchInit(struct Widget *widget, GLuint texture, int width, int height, float t, float r, float b, float l) {
	struct NinePatch *ninePatch = (struct NinePatch *) widget;
	widgetInitialize(widget);
	widget->vtable = &ninePatchClass;
	float f = MIN(width / (l + r), height / (t + b));
	if (f < 1) {
		t *= f;
		r *= f;
		b *= f;
		l *= f;
	}

	ninePatch->texture = texture;
	ninePatch->width = width;
	ninePatch->height = height;
	ninePatch->padTop = ninePatch->t = t;
	ninePatch->padRight = ninePatch->r = r;
	ninePatch->padBottom = ninePatch->b = b;
	ninePatch->padLeft = ninePatch->l = l;

	ninePatch->child = 0;
}
