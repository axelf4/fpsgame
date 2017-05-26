#include "spriteBatch.h"
#include <stdlib.h>
#include <string.h>
#include "glUtil.h"

/**
 * Number of vertices per sprite in sprite batch.
 */
#define SPRITE_SIZE 16

void *alignedAlloc(size_t size, size_t align) {
	return _mm_malloc(size, align);
}

void alignedFree(void *ptr) {
	_mm_free(ptr);
}

struct SpriteBatch *spriteBatchCreate(int size) {
	struct SpriteBatch *renderer = alignedAlloc(sizeof(struct SpriteBatch), 16);
	if (!renderer) {
		return 0;
	}
	renderer->vertices = malloc(sizeof(float) * SPRITE_SIZE * size);
	unsigned short *indices = malloc(sizeof(unsigned short) * 6 * size);
	for (int i = 0, j = 0, length = size * 6; i < length; i += 6, j += 4) {
		indices[i] = j + 0;
		indices[i + 1] = j + 1;
		indices[i + 2] = j + 2;
		indices[i + 3] = j + 0;
		indices[i + 4] = j + 2;
		indices[i + 5] = j + 3;
	}
	renderer->index = 0;
	renderer->maxVertices = size * SPRITE_SIZE;
	renderer->drawing = 0;

	glGenBuffers(1, &renderer->vertexObject);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vertexObject);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * SPRITE_SIZE * size, renderer->vertices, GL_STREAM_DRAW);
	glGenBuffers(1, &renderer->indexObject);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->indexObject);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * 6 * size, indices, GL_STATIC_DRAW);
	free(indices);

	const GLchar *vertexShaderSource = "uniform mat4 projection;"
		"attribute vec2 vertex;"
		"attribute vec2 tex_coord;"
		"varying vec2 vTexCoord;"
		"void main() {"
		"	vTexCoord = tex_coord;"
		"	gl_Position = projection * vec4(vertex, 0.0, 1.0);"
		"}",
		*fragmentShaderSource = "uniform sampler2D texture;"
			"varying vec4 vColor;"
			"varying vec2 vTexCoord;"
			"void main() {"
			"	gl_FragColor = texture2D(texture, vTexCoord);"
			"}";
	renderer->defaultProgram = renderer->program =
		createProgram(vertexShaderSource, fragmentShaderSource);
	glLinkProgram(renderer->program);

	const GLchar *textVertexShaderSource = "uniform mat4 projection;"
		"attribute vec2 vertex;"
		"attribute vec2 tex_coord;"
		"varying vec2 vTexCoord;"
		"void main() {"
		"	vTexCoord = tex_coord;"
		"	gl_Position = projection * vec4(vertex, 0.0, 1.0);"
		"}",
		*textFragmentShaderSource = "uniform sampler2D texture;"
			"uniform vec4 color;"
			"varying vec2 vTexCoord;"
			"void main() {"
			"	float a = texture2D(texture, vTexCoord).r;"
			"	gl_FragColor = vec4(color.rgb, color.a * a);"
			"}";
	renderer->textProgram = createProgram(textVertexShaderSource, textFragmentShaderSource);
	glLinkProgram(renderer->textProgram);

	return renderer;
}

void spriteBatchDestroy(struct SpriteBatch *renderer) {
	glDeleteBuffers(1, &renderer->vertexObject);
	glDeleteBuffers(1, &renderer->indexObject);
	free(renderer->vertices);
	glDeleteProgram(renderer->defaultProgram);
	glDeleteProgram(renderer->textProgram);
	alignedFree(renderer);
}

static void spriteBatchSetupProgram(struct SpriteBatch *renderer) {
	ALIGN(16) float mv[16];
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "projection"), 1, 0, MatrixGet(mv, renderer->projectionMatrix));
	glUniform1i(glGetUniformLocation(renderer->program, "texture"), 0);
	renderer->vertexAttrib = glGetAttribLocation(renderer->program, "vertex");
	renderer->texCoordAttrib = glGetAttribLocation(renderer->program, "tex_coord");
}

void spriteBatchBegin(struct SpriteBatch *renderer) {
	renderer->drawing = 1;
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vertexObject);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->indexObject);

	glActiveTexture(GL_TEXTURE0);

	glUseProgram(renderer->program);
	spriteBatchSetupProgram(renderer);

	glEnableVertexAttribArray(renderer->vertexAttrib);
	glEnableVertexAttribArray(renderer->texCoordAttrib);
}

static void spriteBatchFlush(struct SpriteBatch *renderer) {
	if (renderer->index == 0) return;
	GLsizei stride = sizeof(float) * 4;
	glVertexAttribPointer(renderer->vertexAttrib, 2, GL_FLOAT, GL_FALSE, stride, 0);
	glVertexAttribPointer(renderer->texCoordAttrib, 2, GL_FLOAT, GL_FALSE, stride, BUFFER_OFFSET(sizeof(float) * 2));
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * renderer->index, renderer->vertices);
	glDrawElements(GL_TRIANGLES, renderer->index / 16 * 6, GL_UNSIGNED_SHORT, 0);
	renderer->index = 0;
}

void spriteBatchEnd(struct SpriteBatch *renderer) {
	spriteBatchFlush(renderer);
	glDisableVertexAttribArray(renderer->vertexAttrib);
	glDisableVertexAttribArray(renderer->texCoordAttrib);
	renderer->lastTexture = 0;
	renderer->drawing = 0;
}

void spriteBatchSwitchProgram(struct SpriteBatch *renderer, GLuint program) {
	if (renderer->drawing) {
		spriteBatchFlush(renderer);
	}
	renderer->program = program ? program : renderer->defaultProgram;
	if (renderer->drawing) {
		glUseProgram(renderer->program);
		spriteBatchSetupProgram(renderer);
	}
}

static void spriteBatchSwitchTexture(struct SpriteBatch *renderer, GLuint texture) {
	spriteBatchFlush(renderer);
	glBindTexture(GL_TEXTURE_2D, texture);
	renderer->lastTexture = texture;
}

void spriteBatchDraw(struct SpriteBatch *renderer, GLuint texture, float x, float y, float width, float height) {
	if (texture != renderer->lastTexture) {
		spriteBatchSwitchTexture(renderer, texture);
	} else if (renderer->index == renderer->maxVertices) {
		spriteBatchFlush(renderer);
	}

	float x0 = x, y0 = y, x1 = x + width, y1 = y + height;
	float *vertices = renderer->vertices;
	vertices[renderer->index + 0] = x0;
	vertices[renderer->index + 1] = y0;
	vertices[renderer->index + 2] = 0;
	vertices[renderer->index + 3] = 0;

	vertices[renderer->index + 4] = x0;
	vertices[renderer->index + 5] = y1;
	vertices[renderer->index + 6] = 0;
	vertices[renderer->index + 7] = 1;

	vertices[renderer->index + 8] = x1;
	vertices[renderer->index + 9] = y1;
	vertices[renderer->index + 10] = 1;
	vertices[renderer->index + 11] = 1;

	vertices[renderer->index + 12] = x1;
	vertices[renderer->index + 13] = y0;
	vertices[renderer->index + 14] = 1;
	vertices[renderer->index + 15] = 0;
	renderer->index += 16;
}

void spriteBatchDrawCustom(struct SpriteBatch *renderer, GLuint texture, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1) {
	if (texture != renderer->lastTexture) {
		spriteBatchSwitchTexture(renderer, texture);
	} else if (renderer->index == renderer->maxVertices) {
		spriteBatchFlush(renderer);
	}

	float *vertices = renderer->vertices;
	vertices[renderer->index + 0] = x0;
	vertices[renderer->index + 1] = y0;
	vertices[renderer->index + 2] = s0;
	vertices[renderer->index + 3] = t0;

	vertices[renderer->index + 4] = x0;
	vertices[renderer->index + 5] = y1;
	vertices[renderer->index + 6] = s0;
	vertices[renderer->index + 7] = t1;

	vertices[renderer->index + 8] = x1;
	vertices[renderer->index + 9] = y1;
	vertices[renderer->index + 10] = s1;
	vertices[renderer->index + 11] = t1;

	vertices[renderer->index + 12] = x1;
	vertices[renderer->index + 13] = y0;
	vertices[renderer->index + 14] = s1;
	vertices[renderer->index + 15] = t0;
	renderer->index += 16;
}

void spriteBatchDrawLayout(struct SpriteBatch *batch, struct Layout *layout, Color color, float x, float y) {
	float r = color.r, g = color.g, b = color.b, a = color.a;
	struct Font *font = layout->font;

	spriteBatchSwitchProgram(batch, batch->textProgram);
	glUniform4f(glGetUniformLocation(batch->textProgram, "color"), r, g, b, a);

	float penX = x;

	for (int i = 0; i < layout->lineCount; ++i) {
		struct LayoutLine line = layout->lines[i];
		for (int j = 0; j < line.itemCount; ++j) {
			struct GlyphString *glyphs = line.items[j]->glyphs;
			for (int k = 0; k < glyphs->length; ++k) {
				struct GlyphInfo info = glyphs->infos[k];
				struct Glyph *glyph = fontGetGlyph(font, info.glyph);
				if (glyph) {
					int x0 = penX + info.xOffset,
						y0 = y + font->ascender - glyph->offsetY,
						x1 = x0 + glyph->width,
						y1 = y0 + glyph->height;
					spriteBatchDrawCustom(batch, font->texture,
							x0, y0, x1, y1,
							glyph->s0, glyph->t0, glyph->s1, glyph->t1);
					penX += info.width;
				}
			}
		}
		penX = x;
		y += font->lineSpacing;
	}

	spriteBatchSwitchProgram(batch, 0);
}
