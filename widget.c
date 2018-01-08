#include "widget.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

void widgetInitialize(struct Widget *widget) {
	widget->x = widget->y = widget->width = widget->height = 0;
	widget->flags = WIDGET_LAYOUT_REQUIRED;
	widget->layoutParams = 0;
	widget->next = widget->child = widget->parent = 0;
	widget->focusability = FOCUS_BEFORE_DESCENDANTS;

	struct ListenerList *list = &widget->listeners;
	list->count = 0;
	list->listeners = 0;
}

void widgetDestroy(struct Widget *widget) {
	free(widget->listeners.listeners);
}

void widgetSetLayoutParams(struct Widget *widget, void *layoutParams) {
	widget->layoutParams = layoutParams;
}

void widgetMarkValidated(struct Widget *widget) {
	widget->flags &= ~WIDGET_LAYOUT_REQUIRED;
}

void widgetRequestLayout(struct Widget *widget) {
	widget->flags |= WIDGET_LAYOUT_REQUIRED;

	if (widget->parent) {
		widgetRequestLayout(widget->parent);
	}
}

void widgetValidate(struct Widget *widget, float width, float height) {
	if (widget->flags & WIDGET_LAYOUT_REQUIRED) {
		widget->vtable->layout(widget, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);
	}
}

void widgetLayout(struct Widget *widget, float width, enum MeasureMode widthMode, float height, enum MeasureMode heightMode) {
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

int widgetIsDescendant(struct Widget *widget, struct Widget *child) {
	do if (child == widget) return 1; while ((child = child->parent));
	return 0;
}

void widgetAddListener(struct Widget *widget, struct Listener listener) {
	struct ListenerList *list = &widget->listeners;
	list->listeners = realloc(list->listeners, sizeof listener * ++list->count);
	list->listeners[list->count - 1] = listener;
}

static int widgetContainsPoint(struct Widget *widget, float x, float y) {
	return x >= widget->x && y >= widget->y && x <= widget->x + widget->width && y <= widget->y + widget->height;
}

/**
 * Returns the deepest widget that contains the point or \c 0.
 * The coordinates are specified in the parent widget's coordinate system.
 */
static struct Widget *getIntersectedWidget(struct Widget *widget, float x, float y) {
	int hit = widgetContainsPoint(widget, x, y);
	if (!hit) return 0;
	struct Widget *result = widget, *child = widget->child;
	while (child) {
		struct Widget *childHit = getIntersectedWidget(child, x - widget->x, y - widget->y);
		if (childHit) result = childHit;
		child = child->next;
	}
	return result;
}

enum EventPhase {
	EVENT_CAPTURING,
	EVENT_AT_TARGET,
	EVENT_BUBBLING
};

static void notifyWidget(struct Widget *widget, struct Event *event, enum EventPhase phase) {
	for (int i = 0; i < widget->listeners.count && !event->canceled; ++i) {
		struct Listener *listener = widget->listeners.listeners + i;
		if ((listener->useCapture == (phase == EVENT_CAPTURING)
					|| phase == EVENT_AT_TARGET)
				&& strcmp(event->eventName, listener->eventName) == 0)
			listener->callback(widget, event, listener->data);
	}
}

void widgetDispatchEvent(struct Widget *widget, struct Event *event) {
	int numParents = 0;
	struct Widget *parent = widget;
	while ((parent = parent->parent)) ++numParents;
	struct Widget *parents[numParents];
	parent = widget;
	for (int i = 0; i < numParents; ++i) {
		parents[i] = parent = parent->parent;
	}

	// Capture
	for (int i = numParents; i-- > 0;) {
		notifyWidget(parents[i], event, EVENT_CAPTURING);
		if (event->canceled) return;
	}

	// Target
	notifyWidget(widget, event, EVENT_AT_TARGET);
	if (event->canceled) return;

	// Bubble
	if (event->bubbles)
		for (int i = 0; i < numParents; ++i) {
			notifyWidget(parents[i], event, EVENT_BUBBLING);
			if (event->canceled) return;
		}
}

static int requestFocusInDescendants(struct GuiContext *context, struct Widget *widget) {
	struct Widget *child = widget->child;
	while (child) {
		if (widgetRequestFocus(context, child)) return 1;
		child = child->next;
	}
	return 0;
}

int widgetRequestFocus(struct GuiContext *context, struct Widget *widget) {
	// If an ancestor is blocking focus
	struct Widget *parent = widget;
	while (parent->parent)
		if ((parent = parent->parent)->focusability == FOCUS_BLOCK_DESCENDANTS)
			return 0;

	// If the root of the isnt the same as the context
	// TODO maybe allow root in the middle of tree?
	if (parent != context->root)
		return 0;

	if (widget->focusability == FOCUS_AFTER_DESCENDANTS
			&& requestFocusInDescendants(context, widget))
		return 1;

	if (widget->flags & WIDGET_FOCUSABLE) {
		context->focused = widget;
		return 1;
	}

	if (widget->focusability == FOCUS_BEFORE_DESCENDANTS
			&& requestFocusInDescendants(context, widget))
		return 1;

	return 0;
}

void guiUpdate(struct GuiContext *context, float x, float y) {
	struct Widget *mouseOverLast = context->mouseOver,
				  *mouseOver = getIntersectedWidget(context->root, x, y);
	if (mouseOverLast != mouseOver) {
		if (mouseOverLast)
			widgetDispatchEvent(mouseOverLast, &(struct Event) { "mouseExit", mouseOverLast, 1 });
		if (mouseOver)
			widgetDispatchEvent(mouseOver, &(struct Event) { "mouseEnter", mouseOver, 1 });
		context->mouseOver = mouseOver;
	}
}

void guiDraw(struct GuiContext *context, struct SpriteBatch *batch, float width, float height) {
	widgetValidate(context->root, width, height);
	widgetDraw(context->root, batch);
}

void guiSetRoot(struct GuiContext *context, struct Widget *widget) {
	context->root = widget;
	context->mouseOver = context->focused = 0;
	memset(context->mouseFoci, 0, MOUSE_BTN_COUNT);
}

void guiMouseDown(struct GuiContext *context, enum MouseButton button, float x, float y) {
	struct Widget *hit = getIntersectedWidget(context->root, x, y);
	if (hit) {
		widgetDispatchEvent(hit, &(struct Event) { "mouseDown", hit, 1 });
		context->mouseFoci[button] = hit;
		if (!widgetRequestFocus(context, hit)) context->focused = 0;
	}
}

void guiMouseUp(struct GuiContext *context, enum MouseButton button, float x, float y) {
	struct Widget *mouseFoci = context->mouseFoci[button];
	assert(mouseFoci && "Button not yet pressed.");
	struct Widget *hit = getIntersectedWidget(context->root, x, y);
	widgetDispatchEvent(mouseFoci, &(struct Event) { "mouseUp", mouseFoci, 1 });
	if (mouseFoci == hit) {
		widgetDispatchEvent(hit, &(struct Event) { "mouseClick", hit, 1 });
	}
	context->mouseFoci[button] = 0;
}

void guiKeyDown(struct GuiContext *context, SDL_Scancode scancode) {
	if (context->focused) {
		// TODO send scancode with key event
		widgetDispatchEvent(context->focused, (struct Event *) &(struct KeyEvent) { { "keyDown", context->focused, 1}, scancode });
	}
}

static void flexContextWidgetSetX(const void *widget, float x) {
	((struct Widget *) widget)->x = x;
}

static void flexContextWidgetSetY(const void *widget, float y) {
	((struct Widget *) widget)->y = y;
}

static float flexContextWidgetGetWidth(const void *widget) {
	return ((struct Widget *) widget)->width;
}

static void flexContextWidgetSetWidth(const void *widget, float width) {
	((struct Widget *) widget)->width = width;
}

static float flexContextWidgetGetHeight(const void *widget) {
	return ((struct Widget *) widget)->height;
}

static void flexContextWidgetSetHeight(const void *widget, float height) {
	((struct Widget *) widget)->height = height;
}

static void flexContextWidgetLayout(const void *widget, float width, enum MeasureMode widthMode, float height, enum MeasureMode heightMode) {
	((struct Widget *) widget)->vtable->layout((struct Widget *) widget, width, widthMode, height, heightMode);
}

static int flexContextWidgetGetChildCount(const void *widget) {
	int i = 0;
	struct Widget *child = ((struct Widget *) widget)->child;
	while (child) {
		++i;
		child = child->next;
	}
	return i;
}

static void *flexContextWidgetGetChildAt(const void *widget, int index) {
	struct Widget *child = ((struct Widget *) widget)->child;
	for (; index > 0; --index) child = child->next;
	return child;
}

static void *flexContextWidgetGetLayoutParams(const void *widget) {
	return ((struct Widget *) widget)->layoutParams;
}

static const struct FlexContext flexContext = {
	flexContextWidgetSetX,
	flexContextWidgetSetY,
	flexContextWidgetGetWidth,
	flexContextWidgetSetWidth,
	flexContextWidgetGetHeight,
	flexContextWidgetSetHeight,
	flexContextWidgetLayout,
	flexContextWidgetGetChildCount,
	flexContextWidgetGetChildAt,
	flexContextWidgetGetLayoutParams,
};

static void flexLayoutLayout(struct Widget *widget, float width, enum MeasureMode widthMode, float height, enum MeasureMode heightMode) {
	struct FlexLayout *flexLayout = (struct FlexLayout *) widget;
	layoutFlex(&flexContext, widget, width, widthMode, height, heightMode, flexLayout->direction, flexLayout->justify);
	widgetMarkValidated(widget);
}

static struct WidgetClass flexLayoutClass = {
	flexLayoutLayout, widgetDrawChildren
};

void flexLayoutInitialize(struct Widget *widget, enum FlexDirection direction, enum Align justify) {
	struct FlexLayout *flexLayout = (struct FlexLayout *) widget;
	widgetInitialize(widget);
	widget->vtable = &flexLayoutClass;
	flexLayout->direction = direction;
	flexLayout->justify = justify;
}
