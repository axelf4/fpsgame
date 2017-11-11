#include "widget.h"
#include "box.h"
#include "label.h"

struct Button {
	struct Box box;
	struct Label label;
	struct ColorDrawable background;
	int over : 1, pressed : 1;
};

void buttonInit(struct Widget *widget, struct Font *font, const char *text);

void buttonDestroy(struct Widget *widget);
