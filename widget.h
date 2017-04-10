#ifndef WIDGET_H
#define WIDGET_H

#include <flexLayout.h>
#include <GL/glew.h>
#include "renderer.h"

struct Widget;

enum {
	WIDGET_FLAG_LAYOUT_REQUIRED = 0x1,
};

typedef struct WidgetClass {
	void (*layout)(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode);
	void (*draw)(struct Widget *widget, struct SpriteRenderer *renderer);
} WidgetClass;

typedef struct Widget {
	struct WidgetClass *vtable;
	float x, y, width, height;
	unsigned flags;
	void *layoutParams;
	struct Widget *parent;
} Widget;

void widgetInitialize(struct Widget *widget);

void widgetSetLayoutParams(struct Widget *widget, void *layoutParams);

void widgetMarkValidated(struct Widget *widget);

void widgetRequestLayout(struct Widget *widget);

/**
 * Layout widget, if and only if needed, to the exact specified dimensions.
 */
void widgetValidate(struct Widget *widget, float width, float height);

typedef struct Container {
	struct Widget widget;
	int childCount;
	struct Widget **children;
} Container;

void containerInitialize(struct Widget *widget);

void containerDestroy(struct Widget *widget);

void containerAddChild(struct Widget *container, struct Widget *child);

typedef struct FlexLayout {
	struct Container container;
	FlexDirection direction;
	Align justify;
} FlexLayout;

void flexLayoutInitialize(struct Widget *widget, FlexDirection direction, Align justify);

#endif
