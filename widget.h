#ifndef WIDGET_H
#define WIDGET_H

#include <flexLayout.h>
#include <GL/glew.h>
#include "spriteBatch.h"
#include <SDL.h>

enum {
	WIDGET_LAYOUT_REQUIRED = 0x1,
	WIDGET_FOCUSABLE = 0x10,
};

enum Focusability {
	/** This view will get focus only if none of its descendants want it. */
	FOCUS_AFTER_DESCENDANTS,
	/** This view will get focus before any of its descendants. */
	FOCUS_BEFORE_DESCENDANTS,
	/** This view will block any of its descendants from getting focus, even if they are focusable. */
	FOCUS_BLOCK_DESCENDANTS
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

struct KeyEvent {
	struct Event event;
	SDL_Scancode scancode;
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
	void (*layout)(struct Widget *widget, float width, enum MeasureMode widthMode, float height, enum MeasureMode heightMode);
	void (*draw)(struct Widget *widget, struct SpriteBatch *renderer);
};

struct Widget {
	struct WidgetClass *vtable;
	float x, y, width, height;
	unsigned flags;
	void *layoutParams;
	struct Widget *parent, *child, *next;
	enum Focusability focusability;

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

void widgetLayout(struct Widget *widget, float width, enum MeasureMode widthMode, float height, enum MeasureMode heightMode);

void widgetDraw(struct Widget *widget, struct SpriteBatch *renderer);

void widgetAddChild(struct Widget *widget, struct Widget *child);

void widgetSetChild(struct Widget *widget, struct Widget *child);

/**
 * Convenience function to draw all children.
 */
void widgetDrawChildren(struct Widget *widget, struct SpriteBatch *batch);

/**
 * Returns whether the widget is the same or a descendant of the specified widget.
 * @param widget The parent widget.
 * @param child The supposed descendant.
 * @return True if the widget is a descendant otherwise false.
 */
int widgetIsDescendant(struct Widget *widget, struct Widget *child);

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
				  *mouseFoci[MOUSE_BTN_COUNT],
				  *focused;
};

int widgetRequestFocus(struct GuiContext *context, struct Widget *widget);

void guiUpdate(struct GuiContext *context, float x, float y);

void guiDraw(struct GuiContext *context, struct SpriteBatch *batch, float width, float height);

void guiSetRoot(struct GuiContext *context, struct Widget *widget);

void guiMouseDown(struct GuiContext *context, enum MouseButton button, float x, float y);

void guiMouseUp(struct GuiContext *context, enum MouseButton button, float x, float y);

void guiKeyDown(struct GuiContext *context, SDL_Scancode scancode);

struct FlexLayout {
	struct Widget widget;
	enum FlexDirection direction;
	enum Align justify;
};

void flexLayoutInitialize(struct Widget *widget, enum FlexDirection direction, enum Align justify);

#endif
