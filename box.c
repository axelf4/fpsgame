#include "box.h"
#include <assert.h>

static void colorDrawableDraw(struct Drawable *drawable, struct SpriteBatch *batch, float x, float y, float width, float height) {
	struct ColorDrawable *colorDrawable = (struct ColorDrawable *) drawable;
	spriteBatchDrawColor(batch, colorDrawable->color, x, y, width, height);
}

void colorDrawableInit(struct Drawable *drawable, struct Color color) {
	struct ColorDrawable *colorDrawable = (struct ColorDrawable *) drawable;
	drawable->draw = colorDrawableDraw;
	colorDrawable->color = color;
}

static void boxLayout(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
	struct Box *box = (struct Box *) widget;
	assert(widget->child && "Child is null.");
	width -= box->padLeft + box->padRight;
	height -= box->padTop + box->padBottom;
	widgetLayout(widget->child, width, widthMode, height, heightMode);
	widget->width = widget->child->width + box->padLeft + box->padRight;
	widget->height = widget->child->height + box->padTop + box->padBottom;
	widget->child->x = box->padLeft;
	widget->child->y = box->padTop;
	widgetMarkValidated(widget);
}

static void boxDraw(struct Widget *widget, struct SpriteBatch *batch) {
	struct Box *box = (struct Box *) widget;
	if (box->background) {
		box->background->draw(box->background, batch, widget->x, widget->y, widget->width, widget->height);
	}
	assert(widget->child && "The box has no child.");
	widget->child->x += widget->x;
	widget->child->y += widget->y;
	widgetDraw(widget->child, batch);
	widget->child->x -= widget->x;
	widget->child->y -= widget->y;
}

static struct WidgetClass boxClass = {
	boxLayout, boxDraw
};

void boxInit(struct Widget *widget, struct Drawable *background, float pad) {
	struct Box *box = (struct Box *) widget;
	widgetInitialize(widget);
	widget->vtable = &boxClass;
	box->background = background;
	box->padTop = box->padRight = box->padBottom = box->padLeft = pad;
}

void boxSetBackground(struct Widget *widget, struct Drawable *background) {
	((struct Box *) widget)->background = background;
}
