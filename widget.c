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
	widget->next = widget->child = widget->parent = 0;
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

void widgetLayout(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
	widget->vtable->layout(widget, width, widthMode, height, heightMode);
}

void widgetDraw(struct Widget *widget, struct SpriteBatch *batch) {
	widget->vtable->draw(widget, batch);
}

void widgetDrawChildren(struct Widget *widget, struct SpriteBatch *batch) {
	struct Widget *child = widget->child;
	while (child) {
		child->x += widget->x;
		child->y += widget->y;
		child->vtable->draw(child, batch);
		child->x -= widget->x;
		child->y -= widget->y;
		child = child->next;
	}
}

void widgetAddChild(struct Widget *widget, struct Widget *child) {
	struct Widget **cp = &widget->child;
	while (*cp) cp = &(*cp)->next;
	*cp = child;
	child->parent = widget;
}

void widgetSetChild(struct Widget *widget, struct Widget *child) {
	// TODO detach old children
	widget->child = child;
	child->next = 0;
	child->parent = widget;
}

static void layoutContextWidgetSetX(const void *widget, float x) {
	((struct Widget *) widget)->x = x;
}

static void layoutContextWidgetSetY(const void *widget, float y) {
	((struct Widget *) widget)->y = y;
}

static float layoutContextWidgetGetWidth(const void *widget) {
	return ((struct Widget *) widget)->width;
}

static void layoutContextWidgetSetWidth(const void *widget, float width) {
	((struct Widget *) widget)->width = width;
}

static float layoutContextWidgetGetHeight(const void *widget) {
	return ((struct Widget *) widget)->height;
}

static void layoutContextWidgetSetHeight(const void *widget, float height) {
	((struct Widget *) widget)->height = height;
}

static void layoutContextWidgetLayout(const void *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
	((struct Widget *) widget)->vtable->layout((struct Widget *) widget, width, widthMode, height, heightMode);
}

static int layoutContextWidgetGetChildCount(const void *widget) {
	int i = 0;
	struct Widget *child = ((struct Widget *) widget)->child;
	while (child) {
		++i;
		child = child->next;
	}
	return i;
}

static void *layoutContextWidgetGetChildAt(const void *widget, int index) {
	struct Widget *child = ((struct Widget *) widget)->child;
	for (; index > 0; --index) child = child->next;
	return child;
}

static void *layoutContextWidgetGetLayoutParams(const void *widget) {
	return ((struct Widget *) widget)->layoutParams;
}

static const struct LayoutContext layoutContext = {
	layoutContextWidgetSetX,
	layoutContextWidgetSetY,
	layoutContextWidgetGetWidth,
	layoutContextWidgetSetWidth,
	layoutContextWidgetGetHeight,
	layoutContextWidgetSetHeight,
	layoutContextWidgetLayout,
	layoutContextWidgetGetChildCount,
	layoutContextWidgetGetChildAt,
	layoutContextWidgetGetLayoutParams,
};

static void flexLayoutLayout(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
	struct FlexLayout *flexLayout = (struct FlexLayout *) widget;
	layoutFlex(&layoutContext, widget, width, widthMode, height, heightMode, flexLayout->direction, flexLayout->justify);
	widgetMarkValidated(widget);
}

static struct WidgetClass flexLayoutClass = {
	flexLayoutLayout, widgetDrawChildren
};

void flexLayoutInitialize(struct Widget *widget, FlexDirection direction, Align justify) {
	struct FlexLayout *flexLayout = (struct FlexLayout *) widget;
	widgetInitialize(widget);
	widget->vtable = &flexLayoutClass;
	flexLayout->direction = direction;
	flexLayout->justify = justify;
}
