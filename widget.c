#include "widget.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

void widgetInitialize(struct Widget *widget) {
	widget->x = widget->y = widget->width = widget->height = 0;
	widget->flags = WIDGET_FLAG_LAYOUT_REQUIRED;
	widget->layoutParams = 0;
	widget->parent = 0;
}

void widgetSetLayoutParams(struct Widget *widget, void *layoutParams) {
	widget->layoutParams = layoutParams;
}

void widgetMarkValidated(struct Widget *widget) {
	widget->flags &= ~WIDGET_FLAG_LAYOUT_REQUIRED;
}

void widgetRequestLayout(struct Widget *widget) {
	widget->flags |= WIDGET_FLAG_LAYOUT_REQUIRED;

	if (widget->parent) {
		widgetRequestLayout(widget->parent);
	}
}

void widgetValidate(struct Widget *widget, float width, float height) {
	if (widget->flags & WIDGET_FLAG_LAYOUT_REQUIRED) {
		widget->vtable->layout(widget, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);
	}
}

void containerInitialize(struct Widget *widget) {
	struct Container *container = (struct Container *) widget;
	widgetInitialize(widget);
	container->childCount = 0;
	container->children = malloc(sizeof(struct Widget *) * 8); // FIXME allow for resizing
}

void containerDestroy(struct Widget *widget) {
	struct Container *container = (struct Container *) widget;
	free(container->children);
}

void containerAddChild(struct Widget *widget, struct Widget *child) {
	assert(!child->parent && "The child already has a parent.");
	struct Container *container = (struct Container *) widget;
	container->children[container->childCount++] = child;
	child->parent = widget;
}

/**
 * Simply draws all children.
 */
static void containerDraw(struct Widget *widget, struct SpriteRenderer *renderer) {
	struct Container *container = (struct Container *) widget;
	for (int i = 0; i < container->childCount; ++i) {
		struct Widget *child = container->children[i];
		child->vtable->draw(child, renderer);
	}
}

static void widgetSetX(const void *widget, float x) {
	((struct Widget *) widget)->x = x;
}

static void widgetSetY(const void *widget, float y) {
	((struct Widget *) widget)->y = y;
}

static float widgetGetWidth(const void *widget) {
	return ((struct Widget *) widget)->width;
}

static void widgetSetWidth(const void *widget, float width) {
	((struct Widget *) widget)->width = width;
}

static float widgetGetHeight(const void *widget) {
	return ((struct Widget *) widget)->height;
}

static void widgetSetHeight(const void *widget, float height) {
	((struct Widget *) widget)->height = height;
}

static void widgetLayout(const void *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
	((struct Widget *) widget)->vtable->layout((struct Widget *) widget, width, widthMode, height, heightMode);
}

static int widgetGetChildCount(const void *widget) {
	return ((struct Container *) widget)->childCount;
}

static void *widgetGetChildAt(const void *widget, int index) {
	return ((struct Container *) widget)->children[index];
}

static void *widgetGetLayoutParams(const void *widget) {
	return ((struct Widget *) widget)->layoutParams;
}

static const struct LayoutContext layoutContext = {
	widgetSetX,
	widgetSetY,
	widgetGetWidth,
	widgetSetWidth,
	widgetGetHeight,
	widgetSetHeight,
	widgetLayout,
	widgetGetChildCount,
	widgetGetChildAt,
	widgetGetLayoutParams,
};

static void flexLayoutLayout(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
	struct FlexLayout *flexLayout = (struct FlexLayout *) widget;
	layoutFlex(&layoutContext, widget, width, widthMode, height, heightMode, flexLayout->direction, flexLayout->justify);
	widgetMarkValidated(widget);
}

static struct WidgetClass flexLayoutClass = {
	flexLayoutLayout, containerDraw
};

void flexLayoutInitialize(struct Widget *widget, FlexDirection direction, Align justify) {
	struct FlexLayout *flexLayout = (struct FlexLayout *) widget;
	containerInitialize(widget);
	widget->vtable = &flexLayoutClass;
	flexLayout->direction = direction;
	flexLayout->justify = justify;
}
