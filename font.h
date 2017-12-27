#ifndef FONT_H
#define FONT_H

#include <stddef.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <GL/glew.h>
#include "stb_rect_pack.h"

typedef struct Glyph {
	int codepoint;
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
	GLuint texture;
	size_t numGlyphs, glyphCapacity;
	GLvoid *data;
	// Size of texture
	int dataWidth, dataHeight;
	struct Glyph *glyphs;
	struct stbrp_context stbrp_context;
	struct stbrp_node *nodes;
	int ascender;
	int lineSpacing;
	FT_Face face;
	hb_font_t *hbFont;
	FT_Library library; // TODO break this out
} Font;

/**
 * @return On success zero is returned.
 */
int fontInit(struct Font *font, const char *filename, int width, int height);

struct Glyph *fontGetGlyph(struct Font *font, unsigned int codepoint);

void fontDestroy(struct Font *font);

#endif
