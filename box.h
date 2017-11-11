#ifndef BOX_H
#define BOX_H

#include "widget.h"

struct Drawable {
	void (*draw)(struct Drawable *drawable, struct SpriteBatch *batch, float x, float y, float width, float height);
};

struct ColorDrawable {
	struct Drawable drawable;
	struct Color color;
};

void colorDrawableInit(struct Drawable *drawable, struct Color color);

struct Box {
	struct Widget widget;
	struct Widget *child;
	/** Allowed to be NULL. */
	struct Drawable *background;
	float padTop, padRight, padBottom, padLeft;
};

/**
 * @param background A drawable which constitutes the background, or NULL.
 */
void boxInit(struct Widget *widget, struct Drawable *background, float pad);

void boxSetChild(struct Widget *widget, struct Widget *child);

void boxSetBackground(struct Widget *widget, struct Drawable *background);

#endif
