#ifndef FONT_H
#define FONT_H

#include <gl/glew.h>

typedef struct Glyph {
	char character;
	int width;
	int height;
	float offsetX,
		  offsetY;
	float s0;
	float t0;
	float s1;
	float t1;
	int advanceX;
	int advanceY;
} Glyph;

typedef struct Font {
	int height;
	int ascender, descender;
	int lineGap;
	GLuint texture;
	int numGlyphs;
	struct Glyph *glyphs;
} Font;

struct Font *loadFont(char *filename, int width, int height);

void fontDestroy(struct Font *font);

#endif
