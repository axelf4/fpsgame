#include "button.h"
#include <stdio.h>

static void updateButtonColor(struct Button *button) {
	button->background.color = button->pressed ? (struct Color) { 0.6, 0.2, 0.6, 1.0 } :
		button->over ? (struct Color) { 0.2, 0.9, 0.3, 1.0 } :
		(struct Color) { 0.2, 0.6, 0.7, 1.0 };
}

static void buttonMouseEnter(struct Widget *widget, struct Event *event, void *data) {
	struct Button *button = (struct Button *) widget;
	button->over = 1;
	updateButtonColor(button);
}

static void buttonMouseExit(struct Widget *widget, struct Event *event, void *data) {
	struct Button *button = (struct Button *) widget;
	button->over = 0;
	updateButtonColor(button);
}

static void buttonMouseDown(struct Widget *widget, struct Event *event, void *data) {
	struct Button *button = (struct Button *) widget;
	button->pressed = 1;
	updateButtonColor(button);
	event->canceled = 1;
}

static void buttonMouseUp(struct Widget *widget, struct Event *event, void *data) {
	struct Button *button = (struct Button *) widget;
	button->pressed = 0;
	updateButtonColor(button);
	event->canceled = 1;
}

void buttonInit(struct Widget *widget, struct Font *font, const char *text) {
	struct Button *button = (struct Button *) widget;
	colorDrawableInit((struct Drawable *) &button->background, (struct Color) { 0.2, 0.6, 0.7, 1.0 });
	boxInit(widget, (struct Drawable *) &button->background, 5);

	button->over = button->pressed = 0;

	struct Widget *label = (struct Widget *) &button->label;
	labelInit(label, font, text);
	widgetSetChild(widget, label);

	widgetAddListener(widget, (struct Listener) { "mouseEnter", buttonMouseEnter, 0, 0 });
	widgetAddListener(widget, (struct Listener) { "mouseExit", buttonMouseExit, 0, 0 });

	widgetAddListener(widget, (struct Listener) { "mouseDown", buttonMouseDown, 0, 1 });
	widgetAddListener(widget, (struct Listener) { "mouseUp", buttonMouseUp, 0, 1 });
}
