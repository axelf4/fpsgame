#include "renderer.h"
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

struct SpriteRenderer *spriteRendererCreate(int size) {
	struct SpriteRenderer *renderer = alignedAlloc(sizeof(struct SpriteRenderer), 16);
	if (!renderer) {
		return 0;
	}
	renderer->vertices = malloc(sizeof(float) * SPRITE_SIZE * size);
	unsigned short *indices = malloc(sizeof(unsigned short) * 6 * size);
	for (int i = 0, j = 0, length = size * 6; i < length; i += 6, j += 4) {
		/*indices[i] = j + 0;
		indices[i + 1] = j + 2;
		indices[i + 2] = j + 1;
		indices[i + 3] = j + 0;
		indices[i + 4] = j + 3;
		indices[i + 5] = j + 2;*/
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
		"}";
	const GLchar *fragmentShaderSource = "uniform sampler2D texture;"
		"varying vec4 vColor;"
		"varying vec2 vTexCoord;"
		"void main() {"
		"	gl_FragColor = texture2D(texture, vTexCoord);"
		"}";
	renderer->defaultProgram = renderer->program =
		createProgram(vertexShaderSource, fragmentShaderSource);
	glLinkProgram(renderer->program);

	return renderer;
}

void spriteRendererDestroy(struct SpriteRenderer *renderer) {
	glDeleteBuffers(1, &renderer->vertexObject);
	glDeleteBuffers(1, &renderer->indexObject);
	free(renderer->vertices);
	glDeleteProgram(renderer->defaultProgram);
	alignedFree(renderer);
}

static void spriteRendererSetupProgram(struct SpriteRenderer *renderer) {
	ALIGN(16) float mv[16];
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "projection"), 1, 0, MatrixGet(mv, renderer->projectionMatrix));
	glUniform1i(glGetUniformLocation(renderer->program, "texture"), 0);
	renderer->vertexAttrib = glGetAttribLocation(renderer->program, "vertex");
	renderer->texCoordAttrib = glGetAttribLocation(renderer->program, "tex_coord");
}

void spriteRendererBegin(struct SpriteRenderer *renderer) {
	renderer->drawing = 1;
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vertexObject);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->indexObject);

	glActiveTexture(GL_TEXTURE0);

	glUseProgram(renderer->program);
	spriteRendererSetupProgram(renderer);

	glEnableVertexAttribArray(renderer->vertexAttrib);
	glEnableVertexAttribArray(renderer->texCoordAttrib);
}

static void spriteRendererFlush(struct SpriteRenderer *renderer) {
	if (renderer->index == 0) return;
	GLsizei stride = sizeof(float) * 4;
	glVertexAttribPointer(renderer->vertexAttrib, 2, GL_FLOAT, GL_FALSE, stride, 0);
	glVertexAttribPointer(renderer->texCoordAttrib, 2, GL_FLOAT, GL_FALSE, stride, BUFFER_OFFSET(sizeof(float) * 2));
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * renderer->index, renderer->vertices);
	glDrawElements(GL_TRIANGLES, renderer->index / 16 * 6, GL_UNSIGNED_SHORT, 0);
	renderer->index = 0;
}

void spriteRendererEnd(struct SpriteRenderer *renderer) {
	spriteRendererFlush(renderer);
	glDisableVertexAttribArray(renderer->vertexAttrib);
	glDisableVertexAttribArray(renderer->texCoordAttrib);
	renderer->lastTexture = 0;
	renderer->drawing = 0;
}

void spriteRendererSwitchProgram(struct SpriteRenderer *renderer, GLuint program) {
	if (renderer->drawing) {
		spriteRendererFlush(renderer);
	}
	renderer->program = program ? program : renderer->defaultProgram;
	if (renderer->drawing) {
		glUseProgram(renderer->program);
		spriteRendererSetupProgram(renderer);
	}
}

static void spriteRendererSwitchTexture(struct SpriteRenderer *renderer, GLuint texture) {
	spriteRendererFlush(renderer);
	glBindTexture(GL_TEXTURE_2D, texture);
	renderer->lastTexture = texture;
}

void spriteRendererDraw(struct SpriteRenderer *renderer, GLuint texture, float x, float y, float width, float height) {
	if (texture != renderer->lastTexture) {
		spriteRendererSwitchTexture(renderer, texture);
	} else if (renderer->index == renderer->maxVertices) {
		spriteRendererFlush(renderer);
	}

	float x0 = x, y0 = y, x1 = x + width, y1 = y + height;
	float *vertices = renderer->vertices;
	vertices[renderer->index + 0] = x0;
	vertices[renderer->index + 1] = y0;
	vertices[renderer->index + 2] = 0;
	vertices[renderer->index + 3] = 1;

	vertices[renderer->index + 4] = x1;
	vertices[renderer->index + 5] = y0;
	vertices[renderer->index + 6] = 1;
	vertices[renderer->index + 7] = 1;

	vertices[renderer->index + 8] = x1;
	vertices[renderer->index + 9] = y1;
	vertices[renderer->index + 10] = 1;
	vertices[renderer->index + 11] = 0;

	vertices[renderer->index + 12] = x0;
	vertices[renderer->index + 13] = y1;
	vertices[renderer->index + 14] = 0;
	vertices[renderer->index + 15] = 0;
	renderer->index += 16;
}

void spriteRendererDrawCustom(struct SpriteRenderer *renderer, GLuint texture, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1) {
	if (texture != renderer->lastTexture) {
		spriteRendererSwitchTexture(renderer, texture);
	} else if (renderer->index == renderer->maxVertices) {
		spriteRendererFlush(renderer);
	}

	float *vertices = renderer->vertices;
	vertices[renderer->index + 0] = x0;
	vertices[renderer->index + 1] = y0;
	vertices[renderer->index + 2] = s0;
	vertices[renderer->index + 3] = t0;

	vertices[renderer->index + 4] = x1;
	vertices[renderer->index + 5] = y0;
	vertices[renderer->index + 6] = s1;
	vertices[renderer->index + 7] = t0;

	vertices[renderer->index + 8] = x1;
	vertices[renderer->index + 9] = y1;
	vertices[renderer->index + 10] = s1;
	vertices[renderer->index + 11] = t1;

	vertices[renderer->index + 12] = x0;
	vertices[renderer->index + 13] = y1;
	vertices[renderer->index + 14] = s0;
	vertices[renderer->index + 15] = t1;
	renderer->index += 16;
}

struct TextRenderer {
	GLuint program;
};

struct TextRenderer *textRendererCreate() {
	struct TextRenderer *renderer = malloc(sizeof(struct TextRenderer));
	if (!renderer) {
		return 0;
	}

	const GLchar *vertexShaderSource = "uniform mat4 projection;"
		"attribute vec2 vertex;"
		"attribute vec2 tex_coord;"
		"varying vec2 vTexCoord;"
		"void main() {"
		"	vTexCoord = tex_coord;"
		"	gl_Position = projection * vec4(vertex, 0.0, 1.0);"
		"}";
	const GLchar *fragmentShaderSource = "uniform sampler2D texture;"
		"uniform vec4 color;"
		"varying vec2 vTexCoord;"
		"void main() {"
		"	float a = texture2D(texture, vTexCoord).r;"
		"	gl_FragColor = vec4(color.rgb, color.a * a);"
		"}";
	renderer->program = createProgram(vertexShaderSource, fragmentShaderSource);
	glLinkProgram(renderer->program);

	return renderer;
}

void textRendererDraw(struct TextRenderer *renderer, struct SpriteRenderer *spriteRenderer, struct Font *font, const char *text, Color color, float x, float y) {
	float r = color.r, g = color.g, b = color.b, a = color.a;

	spriteRendererSwitchProgram(spriteRenderer, renderer->program);
	glUniform4f(glGetUniformLocation(renderer->program, "color"), r, g, b, a);

	for (size_t i = 0, length = strlen(text); i < length; ++i) {
		char c = text[i];
		struct Glyph *glyph;
		for (int j = 0; j < font->numGlyphs; ++j) {
			if (font->glyphs[j].character == c) glyph = font->glyphs + j;
		}
		if (glyph) {
			/*if (i > 0) {
				x += texture_glyph_get_kerning(glyph, text + i - 1);
			}*/
			int x0 = x + glyph->offsetX,
				y0 = y + glyph->offsetY,
				x1 = x0 + glyph->width,
				y1 = y0 - glyph->height;
			float s0 = glyph->s0, t0 = glyph->t0, s1 = glyph->s1, t1 = glyph->t1;
			spriteRendererDrawCustom(spriteRenderer, font->texture,
					x0, y1, x1, y0,
					s0, t1, s1, t0);
			x += glyph->advanceX;
		}
	}

	spriteRendererSwitchProgram(spriteRenderer, 0);
}

void textRendererDestroy(struct TextRenderer *renderer) {
	glDeleteProgram(renderer->program);
	free(renderer);
}
