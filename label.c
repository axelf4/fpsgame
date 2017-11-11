#include "label.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

static void labelLayout(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
	struct Label *label = (struct Label *) widget;
	struct Layout *layout = label->layout;

	if (widthMode == MEASURE_EXACTLY || widthMode == MEASURE_AT_MOST) {
		layoutSetWidth(layout, width);
	} else {
		layoutSetWidth(layout, -1);
	}
	if (heightMode == MEASURE_EXACTLY || heightMode == MEASURE_AT_MOST) {
		layoutSetHeight(layout, height);
	} else {
		layoutSetHeight(layout, -1);
	}

	printf("layout label need width %f height %f %d\n", width, height, widthMode);

	int layoutWidth, layoutHeight;
	layoutLayout(layout);
	layoutGetSize(layout, &layoutWidth, &layoutHeight);
	if (widthMode == MEASURE_EXACTLY) widget->width = width;
	else widget->width = layoutWidth;
	if (heightMode == MEASURE_EXACTLY) widget->height = height;
	else widget->height = layoutHeight;
	printf("layouted label to %f, %f\n", widget->width, widget->height);
	widgetMarkValidated(widget);
}

static void labelDraw(struct Widget *widget, struct SpriteBatch *batch) {
	struct Label *label = (struct Label *) widget;
	struct Color color = {0.96, 0.82, 0.3, 1};
	spriteBatchDrawLayout(batch, label->layout, color,
			widget->x, widget->y);
}

static struct WidgetClass labelClass = {
	labelLayout, labelDraw
};

void labelInit(struct Widget *widget, struct Font *font, const char *text) {
	assert(widget && "widget is null.");
	struct Label *label = (struct Label *) widget;
	widgetInitialize(widget);
	widget->vtable = &labelClass;
	label->font = font;
	label->layout = malloc(sizeof(struct Layout));
	layoutInit(label->layout, font);
	layoutSetText(label->layout, text, -1);
}

void labelDestroy(struct Widget *widget) {
	struct Label *label = (struct Label *) widget;
	widgetDestroy(widget);
	layoutDestroy(label->layout);
	free(label->layout);
}
