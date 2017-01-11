#include "font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

struct Font *loadFont(char *filename, int width, int height) {
	FT_Error error;
	FT_Library library;
	if (error = FT_Init_FreeType(&library)) {
		fprintf(stderr, "Could not initialize FreeType.\n");
		goto cleanup;
	}
	FT_Face face;
	// FT_New_Memory_Face(ft, data, dataSize, 0, &face);
	if (error = FT_New_Face(library, filename, 0, &face)) {
		fprintf(stderr, "Could not open font.\n");
		goto cleanup_library;
	}
	if (error = FT_Set_Char_Size(face, 0, 16 * 64, 300, 300)) {
		fprintf(stderr, "Could set sizes.\n");
		goto cleanup_face;
	}

	struct Font *font = malloc(sizeof(struct Font));
	if (!font) {
		goto cleanup_face;
	}

	FT_Size_Metrics metrics = face->size->metrics;
	font->ascender = metrics.ascender;
	font->descender = metrics.descender;
	font->height = metrics.height;
	font->lineGap = font->height - font->ascender + font->descender;
	FT_GlyphSlot slot = face->glyph;

	GLvoid *data = malloc(width * height);
	if (!data) {
		fprintf(stderr, "Could not allocate image data.\n");
		goto cleanup_face;
	}
	struct stbrp_context stbrp_context;
	const int numNodes = width;
	struct stbrp_node *nodes = malloc(sizeof(struct stbrp_node) * numNodes);
	stbrp_init_target(&stbrp_context, width, height, nodes, numNodes);

	const char cache[] = " !\"#$%&'()*+,-./0123456789:;<=>?"
		"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
		"`abcdefghijklmnopqrstuvwxyz{|}~";
	size_t numGlyphs = strlen(cache);
	struct Glyph *glyphs = malloc(sizeof(struct Glyph) * numGlyphs);
	printf("Loading %d glyphs from %s.\n", numGlyphs, filename);
	for (int i = 0; i < numGlyphs; ++i) {
		const char c = cache[i];
		if (error = FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			fprintf(stderr, "Could not load character.\n");
			goto cleanup_face;
		}
		FT_Bitmap ft_bitmap = slot->bitmap;
		unsigned int srcWidth = ft_bitmap.width,
					 srcHeight = ft_bitmap.rows;

		struct stbrp_rect rect;
		rect.w = srcWidth;
		rect.h = srcHeight;
		stbrp_pack_rects(&stbrp_context, &rect, 1);
		if (!rect.was_packed) {
			fprintf(stderr, "Rect got REKT!\n");
			goto cleanup_face;
		}
		unsigned char *dst = data + rect.x + width * rect.y,
					  *src = ft_bitmap.buffer;
		for (int i = 0; i < srcHeight; ++i) {
			memcpy(dst, src, ft_bitmap.width);
			dst += width;
			src += ft_bitmap.pitch;
		}

		struct Glyph *glyph = glyphs + i;
		glyph->character = c;
		glyph->width = srcWidth;
		glyph->height = srcHeight;
		glyph->offsetX = slot->bitmap_left;
		glyph->offsetY = slot->bitmap_top;
		glyph->s0 = (float) rect.x / width;
		glyph->t0 = (float) rect.y / height;
		glyph->s1 = (float) (rect.x + glyph->width) / width;
		glyph->t1 = (float) (rect.y + glyph->height) / height;
		glyph->advanceX = slot->advance.x >> 6;
		glyph->advanceY = slot->advance.y;
		// TODO keming
	}
	free(nodes);

	glGenTextures(1, &font->texture);
	glBindTexture(GL_TEXTURE_2D, font->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);
	free(data);

	font->numGlyphs = numGlyphs;
	font->glyphs = glyphs;

	return font;

cleanup_face:
	FT_Done_Face(face);
cleanup_library:
	FT_Done_FreeType(library);
cleanup:
	return 0;
}

void fontDestroy(struct Font *font) {
	glDeleteTextures(1, &font->texture);
	free(font->glyphs);
	free(font);
}
