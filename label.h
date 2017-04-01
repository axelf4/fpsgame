#ifndef LABEL_H
#define LABEL_H

#include "widget.h"
#include "font.h"
#include "renderer.h"
#include "linebreak.h"

typedef struct Label {
	struct Widget widget;
	struct Font *font;
	const char *text;
	struct Layout *layout;
	struct TextRenderer *textRenderer;
} Label;

struct Widget *labelNew(struct Font *font, struct TextRenderer *textRenderer, const char *text);

void labelDestroy(struct Widget *label);

#endif
