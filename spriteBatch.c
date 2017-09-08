#include "spriteBatch.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "glUtil.h"

/**
 * Number of vertices per sprite in sprite batch.
 */
#define SPRITE_SIZE 16

void spriteBatchInitialize(struct SpriteBatch *batch, int size) {
	assert(batch && "The spritebatch cannot be null.");
	batch->vertices = malloc(sizeof(float) * SPRITE_SIZE * size);
	unsigned short *indices = malloc(sizeof(unsigned short) * 6 * size);
	for (int i = 0, j = 0, length = size * 6; i < length; i += 6, j += 4) {
		indices[i] = j + 0;
		indices[i + 1] = j + 1;
		indices[i + 2] = j + 2;
		indices[i + 3] = j + 0;
		indices[i + 4] = j + 2;
		indices[i + 5] = j + 3;
	}
	batch->index = 0;
	batch->maxVertices = size * SPRITE_SIZE;
	batch->drawing = 0;

	glGenBuffers(1, &batch->vertexObject);
	glBindBuffer(GL_ARRAY_BUFFER, batch->vertexObject);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * SPRITE_SIZE * size, batch->vertices, GL_STREAM_DRAW);
	glGenBuffers(1, &batch->indexObject);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->indexObject);
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
		*fragmentShaderSource = "#ifdef GL_ES\n"
			"precision mediump float;\n"
			"#endif\n"
			"uniform sampler2D texture;"
			"varying vec2 vTexCoord;"
			"void main() {"
			"	gl_FragColor = texture2D(texture, vTexCoord);"
			"}";
	batch->defaultProgram = batch->program = createProgramVertFrag(vertexShaderSource, fragmentShaderSource);

	const GLchar *textVertexShaderSource = "uniform mat4 projection;"
		"attribute vec2 vertex;"
		"attribute vec2 tex_coord;"
		"varying vec2 vTexCoord;"
		"void main() {"
		"	vTexCoord = tex_coord;"
		"	gl_Position = projection * vec4(vertex, 0.0, 1.0);"
		"}",
		*textFragmentShaderSource = "#ifdef GL_ES\n"
			"precision mediump float;\n"
			"#endif\n"
			"uniform sampler2D texture;"
			"uniform vec4 color;"
			"varying vec2 vTexCoord;"
			"void main() {"
			"	float a = texture2D(texture, vTexCoord).a;"
			"	gl_FragColor = vec4(color.rgb, color.a * a);"
			"}";
	batch->textProgram = createProgramVertFrag(textVertexShaderSource, textFragmentShaderSource);
}

void spriteBatchDestroy(struct SpriteBatch *batch) {
	glDeleteBuffers(1, &batch->vertexObject);
	glDeleteBuffers(1, &batch->indexObject);
	free(batch->vertices);
	glDeleteProgram(batch->defaultProgram);
	glDeleteProgram(batch->textProgram);
}

static void spriteBatchSetupProgram(struct SpriteBatch *batch) {
	ALIGN(16) float mv[16];
	glUniformMatrix4fv(glGetUniformLocation(batch->program, "projection"), 1, 0, MatrixGet(mv, batch->projectionMatrix));
	glUniform1i(glGetUniformLocation(batch->program, "texture"), 0);
	batch->vertexAttrib = glGetAttribLocation(batch->program, "vertex");
	batch->texCoordAttrib = glGetAttribLocation(batch->program, "tex_coord");
}

void spriteBatchBegin(struct SpriteBatch *batch) {
	batch->drawing = 1;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);
	glUseProgram(batch->program);
	spriteBatchSetupProgram(batch);
	glBindBuffer(GL_ARRAY_BUFFER, batch->vertexObject);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->indexObject);
	glEnableVertexAttribArray(batch->vertexAttrib);
	glEnableVertexAttribArray(batch->texCoordAttrib);
}

static void spriteBatchFlush(struct SpriteBatch *batch) {
	if (batch->index == 0) return;
	GLsizei stride = sizeof(float) * 4;
	glVertexAttribPointer(batch->vertexAttrib, 2, GL_FLOAT, GL_FALSE, stride, 0);
	glVertexAttribPointer(batch->texCoordAttrib, 2, GL_FLOAT, GL_FALSE, stride, BUFFER_OFFSET(sizeof(float) * 2));
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * batch->index, batch->vertices);
	glDrawElements(GL_TRIANGLES, batch->index / 16 * 6, GL_UNSIGNED_SHORT, 0);
	batch->index = 0;
}

void spriteBatchEnd(struct SpriteBatch *batch) {
	spriteBatchFlush(batch);
	glDisableVertexAttribArray(batch->vertexAttrib);
	glDisableVertexAttribArray(batch->texCoordAttrib);
	batch->lastTexture = 0;
	batch->drawing = 0;
}

void spriteBatchSwitchProgram(struct SpriteBatch *batch, GLuint program) {
	if (batch->drawing) {
		spriteBatchFlush(batch);
	}
	batch->program = program ? program : batch->defaultProgram;
	if (batch->drawing) {
		glUseProgram(batch->program);
		spriteBatchSetupProgram(batch);
	}
}

static void spriteBatchSwitchTexture(struct SpriteBatch *batch, GLuint texture) {
	spriteBatchFlush(batch);
	glBindTexture(GL_TEXTURE_2D, texture);
	batch->lastTexture = texture;
}

void spriteBatchDraw(struct SpriteBatch *batch, GLuint texture, float x, float y, float width, float height) {
	assert(batch->index <= batch->maxVertices);
	if (texture != batch->lastTexture) {
		spriteBatchSwitchTexture(batch, texture);
	} else if (batch->index == batch->maxVertices) {
		spriteBatchFlush(batch);
	}

	float x0 = x, y0 = y, x1 = x + width, y1 = y + height;
	float *vertices = batch->vertices;
	vertices[batch->index + 0] = x0;
	vertices[batch->index + 1] = y0;
	vertices[batch->index + 2] = 0;
	vertices[batch->index + 3] = 0;

	vertices[batch->index + 4] = x0;
	vertices[batch->index + 5] = y1;
	vertices[batch->index + 6] = 0;
	vertices[batch->index + 7] = 1;

	vertices[batch->index + 8] = x1;
	vertices[batch->index + 9] = y1;
	vertices[batch->index + 10] = 1;
	vertices[batch->index + 11] = 1;

	vertices[batch->index + 12] = x1;
	vertices[batch->index + 13] = y0;
	vertices[batch->index + 14] = 1;
	vertices[batch->index + 15] = 0;
	batch->index += 16;
}

void spriteBatchDrawCustom(struct SpriteBatch *batch, GLuint texture, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1) {
	assert(batch->index <= batch->maxVertices);
	if (texture != batch->lastTexture) {
		spriteBatchSwitchTexture(batch, texture);
	} else if (batch->index == batch->maxVertices) {
		spriteBatchFlush(batch);
	}

	float *vertices = batch->vertices;
	vertices[batch->index + 0] = x0;
	vertices[batch->index + 1] = y0;
	vertices[batch->index + 2] = s0;
	vertices[batch->index + 3] = t0;

	vertices[batch->index + 4] = x0;
	vertices[batch->index + 5] = y1;
	vertices[batch->index + 6] = s0;
	vertices[batch->index + 7] = t1;

	vertices[batch->index + 8] = x1;
	vertices[batch->index + 9] = y1;
	vertices[batch->index + 10] = s1;
	vertices[batch->index + 11] = t1;

	vertices[batch->index + 12] = x1;
	vertices[batch->index + 13] = y0;
	vertices[batch->index + 14] = s1;
	vertices[batch->index + 15] = t0;
	batch->index += 16;
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
