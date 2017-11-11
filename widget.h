#ifndef WIDGET_H
#define WIDGET_H

#include <flexLayout.h>
#include <GL/glew.h>
#include "spriteBatch.h"

enum {
	WIDGET_FLAG_LAYOUT_REQUIRED = 0x1,
};

struct Widget;

typedef struct WidgetClass {
	void (*layout)(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode);
	void (*draw)(struct Widget *widget, struct SpriteBatch *renderer);
} WidgetClass;

struct Widget {
	struct WidgetClass *vtable;
	float x, y, width, height;
	unsigned flags;
	void *layoutParams;
	struct Widget *parent, *child, *next;
};

void widgetInitialize(struct Widget *widget);

void widgetSetLayoutParams(struct Widget *widget, void *layoutParams);

void widgetMarkValidated(struct Widget *widget);

void widgetRequestLayout(struct Widget *widget);

/**
 * Layout widget, if and only if needed, to the exact specified dimensions.
 */
void widgetValidate(struct Widget *widget, float width, float height);

void widgetLayout(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode);

void widgetDraw(struct Widget *widget, struct SpriteBatch *renderer);

void widgetAddChild(struct Widget *widget, struct Widget *child);

void widgetSetChild(struct Widget *widget, struct Widget *child);

/**
 * Convenience function to draw all children.
 */
void widgetDrawChildren(struct Widget *widget, struct SpriteBatch *batch);


struct FlexLayout {
	struct Widget widget;
	FlexDirection direction;
	Align justify;
};

void flexLayoutInitialize(struct Widget *widget, FlexDirection direction, Align justify);

#endif
