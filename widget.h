#ifndef WIDGET_H
#define WIDGET_H

#include <flexLayout.h>
#include <GL/glew.h>
#include "renderer.h"

struct Widget;

typedef struct WidgetClass {
	void (*layout)(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode);
	void (*draw)(struct Widget *widget, struct SpriteRenderer *renderer);
} WidgetClass;

typedef struct Widget {
	struct WidgetClass *vtable;
	float x, y, width, height;
	unsigned flags;
	void *layoutParams;
} Widget;

void widgetInitialize(struct Widget *widget);

void widgetSetLayoutParams(struct Widget *widget, void *layoutParams);

typedef struct Container {
	struct Widget widget;
	int childCount;
	struct Widget **children;
} Container;

void containerInitialize(struct Container *container);

void containerDestroy(struct Container *container);

void containerAddChild(struct Container *container, struct Widget *child);

typedef struct FlexLayout {
	struct Container container;
	FlexDirection direction;
	Align justify;
} FlexLayout;

void flexLayoutInitialize(struct FlexLayout *flexLayout, FlexDirection direction, Align justify);

typedef struct TestWidget {
	struct Widget widget;
	GLuint texture;
} TestWidget;

void testWidgetInitialize(struct TestWidget *testWidget, GLuint texture);

#endif
