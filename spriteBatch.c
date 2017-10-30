#include "spriteBatch.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "glUtil.h"

/**
 * Number of vertices per sprite in sprite batch.
 */
#define SPRITE_SIZE 20

struct Color white = { 1.0f, 1.0f, 1.0f, 1.0f };

void spriteBatchInitialize(struct SpriteBatch *batch, int size) {
	assert(batch && "The spritebatch cannot be null.");
	batch->vertices = malloc(sizeof(GLfloat) * (batch->maxVertices = SPRITE_SIZE * size));
	GLushort *indices = malloc(sizeof(GLushort) * 6 * size);
	for (int i = 0, j = 0, length = size * 6; i < length; i += 6, j += 4) {
		indices[i] = j + 0;
		indices[i + 1] = j + 1;
		indices[i + 2] = j + 2;
		indices[i + 3] = j + 0;
		indices[i + 4] = j + 2;
		indices[i + 5] = j + 3;
	}
	batch->index = 0;
	batch->drawing = 0;

	glGenBuffers(1, &batch->vertexObject);
	glBindBuffer(GL_ARRAY_BUFFER, batch->vertexObject);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * SPRITE_SIZE * size, batch->vertices, GL_STREAM_DRAW);
	glGenBuffers(1, &batch->indexObject);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->indexObject);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * 6 * size, indices, GL_STATIC_DRAW);
	free(indices);

	const GLchar *vertexShaderSource = "uniform mat4 projection;"
		"attribute vec2 vertex;"
		"attribute vec2 tex_coord;"
		"attribute vec4 color;"
		"varying vec2 vTexCoord;"
		"varying vec4 vColor;"
		"void main() {"
		"	vTexCoord = tex_coord;"
		"	vColor = color;"
		"	gl_Position = projection * vec4(vertex, 0.0, 1.0);"
		"}",
		*fragmentShaderSource = "#ifdef GL_ES\n"
			"precision mediump float;\n"
			"#endif\n"
			"uniform sampler2D texture;"
			"varying vec2 vTexCoord;"
			"varying vec4 vColor;"
			"void main() {"
			"	gl_FragColor = vColor * texture2D(texture, vTexCoord);"
			"}";
	batch->defaultProgram = batch->program = createProgramVertFrag(vertexShaderSource, fragmentShaderSource);
}

static int packColor(struct Color color) {
	return (int) (0xFF * color.r) | (int) (0xFF * color.g) << 8 | (int) (0xFF * color.b) << 16 | (int) (0xFF * color.a) << 24;
}

void spriteBatchDestroy(struct SpriteBatch *batch) {
	glDeleteBuffers(1, &batch->vertexObject);
	glDeleteBuffers(1, &batch->indexObject);
	free(batch->vertices);
	glDeleteProgram(batch->defaultProgram);
}

static void spriteBatchSetupProgram(struct SpriteBatch *batch) {
	ALIGN(16) float mv[16];
	glUniformMatrix4fv(glGetUniformLocation(batch->program, "projection"), 1, 0, MatrixGet(mv, batch->projectionMatrix));
	glUniform1i(glGetUniformLocation(batch->program, "texture"), 0);
	batch->vertexAttrib = glGetAttribLocation(batch->program, "vertex");
	batch->texCoordAttrib = glGetAttribLocation(batch->program, "tex_coord");
	batch->colorAttrib = glGetAttribLocation(batch->program, "color");
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
	glEnableVertexAttribArray(batch->colorAttrib);
}

static void spriteBatchFlush(struct SpriteBatch *batch) {
	if (batch->index == 0) return;
	GLsizei stride = sizeof(GLfloat) * 4 + sizeof(GLubyte) * 4;
	glVertexAttribPointer(batch->vertexAttrib, 2, GL_FLOAT, GL_FALSE, stride, 0);
	glVertexAttribPointer(batch->texCoordAttrib, 2, GL_FLOAT, GL_FALSE, stride, BUFFER_OFFSET(sizeof(GLfloat) * 2));
	glVertexAttribPointer(batch->colorAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, BUFFER_OFFSET(sizeof(GLfloat) * 4));
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * batch->index, batch->vertices);
	glDrawElements(GL_TRIANGLES, batch->index / stride * 6, GL_UNSIGNED_SHORT, 0);
	batch->index = 0;
}

void spriteBatchEnd(struct SpriteBatch *batch) {
	assert(batch->drawing && "Call begin before end.");
	spriteBatchFlush(batch);
	glDisableVertexAttribArray(batch->vertexAttrib);
	glDisableVertexAttribArray(batch->texCoordAttrib);
	glDisableVertexAttribArray(batch->colorAttrib);
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
	spriteBatchDrawCustom(batch, texture, x, y, x + width, y + height, 0.0f, 0.0f, 1.0f, 1.0f, white);
}

void spriteBatchDrawCustom(struct SpriteBatch *batch, GLuint texture, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1, struct Color color) {
	assert(batch->index <= batch->maxVertices && sizeof(float) == sizeof(int));
	if (texture != batch->lastTexture) {
		spriteBatchSwitchTexture(batch, texture);
	} else if (batch->index == batch->maxVertices) {
		spriteBatchFlush(batch);
	}

	int packedColor = packColor(color);
	float *vertices = batch->vertices;
	vertices[batch->index + 0] = x0;
	vertices[batch->index + 1] = y0;
	vertices[batch->index + 2] = s0;
	vertices[batch->index + 3] = t0;
	memcpy(vertices + batch->index + 4, &packedColor, sizeof(float));

	vertices[batch->index + 5] = x0;
	vertices[batch->index + 6] = y1;
	vertices[batch->index + 7] = s0;
	vertices[batch->index + 8] = t1;
	memcpy(vertices + batch->index + 9, &packedColor, sizeof(float));

	vertices[batch->index + 10] = x1;
	vertices[batch->index + 11] = y1;
	vertices[batch->index + 12] = s1;
	vertices[batch->index + 13] = t1;
	memcpy(vertices + batch->index + 14, &packedColor, sizeof(float));

	vertices[batch->index + 15] = x1;
	vertices[batch->index + 16] = y0;
	vertices[batch->index + 17] = s1;
	vertices[batch->index + 18] = t0;
	memcpy(vertices + batch->index + 19, &packedColor, sizeof(float));
	batch->index += 20;
}

void spriteBatchDrawLayout(struct SpriteBatch *batch, struct Layout *layout, struct Color color, float x, float y) {
	struct Font *font = layout->font;
	for (int i = 0; i < layout->lineCount; ++i) {
		struct LayoutLine line = layout->lines[i];
		float penX = x;
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
							glyph->s0, glyph->t0, glyph->s1, glyph->t1, color);
					penX += info.width;
				}
			}
		}

		y += font->lineSpacing;
	}
}
