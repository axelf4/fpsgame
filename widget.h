#ifndef WIDGET_H
#define WIDGET_H

#include <flexLayout.h>
#include <GL/glew.h>
#include "spriteBatch.h"

enum {
	WIDGET_FLAG_LAYOUT_REQUIRED = 0x1,
};

struct Widget;

struct Event {
	/** The name of the event. */
	const char *eventName;
	/** The widget that triggered the event. */
	struct Widget *target;
	/** Whether the event bubbles up through the parents. */
	int bubbles : 1;
	/* Whether this event has been canceled. */
	int canceled : 1;
};

/**
 * Callback for listening to events.
 * @param widget The widget that the listener belongs to.
 * @param event The event to handle.
 * @param data User-defined data.
 */
typedef void (*EventCallback)(struct Widget *widget, struct Event *event, void *data);

struct Listener {
	const char *eventName;
	EventCallback callback;
	void *data;
	int useCapture : 1;
};

struct ListenerList {
	int count;
	struct Listener *listeners;
};

struct WidgetClass {
	void (*layout)(struct Widget *widget, float width, MeasureMode widthMode, float height, MeasureMode heightMode);
	void (*draw)(struct Widget *widget, struct SpriteBatch *renderer);
};

struct Widget {
	struct WidgetClass *vtable;
	float x, y, width, height;
	unsigned flags;
	void *layoutParams;
	struct Widget *parent, *child, *next;

	struct ListenerList listeners;
};

void widgetInitialize(struct Widget *widget);

void widgetDestroy(struct Widget *widget);

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

/**
 * Adds the listener to be notified of the specified event type on the widget.
 * @param widget The target widget to get notified of events on.
 * @param listener The listener to add.
 */
void widgetAddListener(struct Widget *widget, struct Listener listener);

/**
 * Dispatches the event to listeners subscribed to that type of event.
 * Listeners are executed in the order they are added.
 */
void widgetDispatchEvent(struct Widget *widget, struct Event *event);

enum MouseButton {
	MOUSE_BTN_LEFT,
	MOUSE_BTN_MIDDLE,
	MOUSE_BTN_RIGHT,
	MOUSE_BTN_COUNT
};

struct GuiContext {
	struct Widget *root,
				  *mouseOver,
				  *mouseFoci[MOUSE_BTN_COUNT];
};

void guiUpdate(struct GuiContext *context, float x, float y);

void guiMouseDown(struct GuiContext *context, enum MouseButton button, float x, float y);

void guiMouseUp(struct GuiContext *context, enum MouseButton button, float x, float y);

struct FlexLayout {
	struct Widget widget;
	FlexDirection direction;
	Align justify;
};

void flexLayoutInitialize(struct Widget *widget, FlexDirection direction, Align justify);

#endif
