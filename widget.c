#include "widget.h"
#include <stdlib.h>
#include <stdio.h>

#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

void widgetInitialize(struct Widget *widget) {
	widget->x = widget->y = widget->width = widget->height = widget->flags = 0;
	widget->layoutParams = 0;
}

void widgetSetLayoutParams(struct Widget *widget, void *layoutParams) {
	widget->layoutParams = layoutParams;
}

void containerInitialize(struct Container *container) {
	widgetInitialize((struct Widget *) container);
	container->childCount = 0;
	container->children = malloc(sizeof(struct Widget *) * 8); // FIXME allow for resizing
}

void containerDestroy(struct Container *container) {
	free(container->children);
}

void containerAddChild(struct Container *container, struct Widget *child) {
	container->children[container->childCount++] = child;
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
}

static struct WidgetClass flexLayoutClass = {
	flexLayoutLayout, containerDraw
};

void flexLayoutInitialize(struct FlexLayout *flexLayout, FlexDirection direction, Align justify) {
	containerInitialize((struct Container *) flexLayout);
	((struct Widget *) flexLayout)->vtable = &flexLayoutClass;
	flexLayout->direction = direction;
	flexLayout->justify = justify;
}

static void testWidgetLayout(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
	printf("layouting rect\n");
	if (widthMode == MEASURE_EXACTLY) {
		widget->width = width;
		printf("setting exaclty to %f\n", width);
	} else if (widthMode == MEASURE_AT_MOST) {
		widget->width = MIN(width, 100);
	} else {
		widget->width = 100;
	}
	if (heightMode == MEASURE_EXACTLY) {
		widget->height = height;
	} else if (heightMode == MEASURE_AT_MOST) {
		widget->height = MIN(height, 100);
	} else {
		widget->height = 100;
	}
	printf("layouting widget to %f,%f from %f,%f\n", widget->width, widget->height, width, height);
	printf("widthMode: %d, heightmode: %d\n", widthMode, heightMode);
}

static void testWidgetDraw(struct Widget *widget, struct SpriteRenderer *renderer) {
	struct TestWidget *testWidget = (struct TestWidget *) widget;
	spriteRendererDraw(renderer, testWidget->texture,
			widget->x, widget->y,
			widget->width, widget->height);
}

static struct WidgetClass testWidgetClass = {
	testWidgetLayout, testWidgetDraw
};

void testWidgetInitialize(struct TestWidget *testWidget, GLuint texture) {
	widgetInitialize((struct Widget *) testWidget);
	testWidget->widget.vtable = &testWidgetClass;
	testWidget->texture = texture;
}
