#include "font.h"
#include <stdlib.h>
#include <string.h>
#include <hb-ft.h>
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

struct Font *loadFont(char *filename, int width, int height) {
	struct Font *font = malloc(sizeof(struct Font));
	if (!font) {
		return 0;
	}

	FT_Error error;
	FT_Library library;
	if (error = FT_Init_FreeType(&library)) {
		fprintf(stderr, "Could not initialize FreeType.\n");
		free(font);
		return 0;
	}
	// FT_Face face;
	// FT_New_Memory_Face(ft, data, dataSize, 0, &face);
	if (error = FT_New_Face(library, filename, 0, &font->face)) {
		fprintf(stderr, "Could not open font.\n");
		return 0;
	}
	const int fontSize = 24;
	if (error = FT_Set_Char_Size(font->face, fontSize * 64, 0, 0, 0)) {
		fprintf(stderr, "Could not set sizes.\n");
		return 0;
	}

	FT_Size_Metrics metrics = font->face->size->metrics;
	font->ascender = metrics.ascender >> 6;
	font->lineSpacing = metrics.height >> 6;
	font->dataWidth = width;
	font->dataHeight = height;
	if (!(font->data = malloc(width * height))) {
		fprintf(stderr, "Could not allocate image data.\n");
		return 0;
	}
	const int numNodes = width;
	font->nodes = malloc(sizeof(struct stbrp_node) * numNodes);
	stbrp_init_target(&font->stbrp_context, width, height, font->nodes, numNodes);

	font->numGlyphs = 0;
	font->glyphs = malloc(sizeof(struct Glyph) * (font->glyphCapacity = 64));

	glGenTextures(1, &font->texture);
	glBindTexture(GL_TEXTURE_2D, font->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, font->dataWidth, font->dataHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);

	font->library = library;
	font->hbFont = hb_ft_font_create_referenced(font->face);

	return font;
}

struct Glyph *fontGetGlyph(struct Font *font, unsigned int codepoint) {
	struct Glyph *glyph = 0;
	for (int i = 0; i < font->numGlyphs; ++i) {
		if (font->glyphs[i].codepoint == codepoint) {
			glyph = font->glyphs + i;
			break;
		}
	}

	if (!glyph) {
		FT_Error error;
		if (error = FT_Load_Glyph(font->face, codepoint, FT_LOAD_RENDER)) {
			fprintf(stderr, "Could not load character.\n");
			return 0;
		}
		FT_GlyphSlot slot = font->face->glyph;
		FT_Bitmap ft_bitmap = slot->bitmap;
		unsigned int srcWidth = ft_bitmap.width, srcHeight = ft_bitmap.rows;

		struct stbrp_rect rect;
		rect.w = srcWidth;
		rect.h = srcHeight;
		stbrp_pack_rects(&font->stbrp_context, &rect, 1);
		if (!rect.was_packed) {
			fprintf(stderr, "Rect got REKT!\n");
			return 0;
		}
		unsigned char *dst = font->data + rect.x + font->dataWidth * rect.y, *src = ft_bitmap.buffer;
		for (int i = 0; i < srcHeight; ++i) {
			memcpy(dst, src, ft_bitmap.width);
			dst += font->dataWidth;
			src += ft_bitmap.pitch;
		}

		if (font->numGlyphs >= font->glyphCapacity) {
			struct Glyph *tmp = realloc(font->glyphs, font->glyphCapacity *= 2);
			if (!tmp) return 0;
			font->glyphs = tmp;
		}
		struct Glyph *glyph = font->glyphs + font->numGlyphs++;
		glyph->codepoint = codepoint;
		glyph->width = srcWidth;
		glyph->height = srcHeight;
		glyph->offsetX = slot->bitmap_left;
		glyph->offsetY = slot->bitmap_top;
		glyph->s0 = (float) rect.x / font->dataWidth;
		glyph->t0 = (float) rect.y / font->dataHeight;
		glyph->s1 = (float) (rect.x + glyph->width) / font->dataWidth;
		glyph->t1 = (float) (rect.y + glyph->height) / font->dataHeight;
		glyph->advanceX = slot->advance.x >> 6;
		glyph->advanceY = slot->advance.y;

		glBindTexture(GL_TEXTURE_2D, font->texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, font->dataWidth, font->dataHeight, GL_ALPHA, GL_UNSIGNED_BYTE, font->data);
	}

	return glyph;
}

void fontDestroy(struct Font *font) {
	glDeleteTextures(1, &font->texture);
	free(font->glyphs);
	free(font->nodes);
	FT_Done_Face(font->face);
	FT_Done_FreeType(font->library);
	free(font);
}
