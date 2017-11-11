#ifndef LABEL_H
#define LABEL_H

#include "widget.h"
#include "font.h"
#include "spriteBatch.h"
#include "linebreak.h"

typedef struct Label {
	struct Widget widget;
	struct Font *font;
	const char *text;
	struct Layout *layout;
} Label;

void labelInit(struct Widget *widget, struct Font *font, const char *text);

void labelDestroy(struct Widget *label);

#endif
