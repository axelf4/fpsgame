#ifndef RENDERER_H
#define RENDERER_H

#include <GL/glew.h>
#include <vmath.h>
#include "font.h"

#include "linebreak.h"

typedef struct Color {
	float r, g, b, a;
} Color;

struct SpriteRenderer {
	GLuint vertexObject, indexObject;
	// Number of sprites able to be stored
	int index;
	int maxVertices;
	// Whether we are drawing
	int drawing;
	float *vertices;
	GLuint program, defaultProgram;
	MATRIX projectionMatrix;
	GLint vertexAttrib, texCoordAttrib;
	GLuint lastTexture;
};

struct SpriteRenderer *spriteRendererCreate(int size);

void spriteRendererDestroy(struct SpriteRenderer *renderer);

void spriteRendererBegin(struct SpriteRenderer *renderer);

void spriteRendererEnd(struct SpriteRenderer *renderer);

void spriteRendererSwitchProgram(struct SpriteRenderer *renderer, GLuint program);

void spriteRendererDraw(struct SpriteRenderer *renderer, GLuint texture, float x, float y, float width, float height);

void spriteRendererDrawCustom(struct SpriteRenderer *renderer, GLuint texture, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1);

struct TextRenderer *textRendererCreate();

void textRendererDraw(struct TextRenderer *renderer, struct SpriteRenderer *spriteRenderer, struct Font *font, const char *text, Color color, float x, float y);

void textRendererDrawLayout(struct TextRenderer *renderer, struct SpriteRenderer *spriteRenderer, struct Layout *layout, Color color, float x, float y);

void textRendererDestroy(struct TextRenderer *renderer);

#endif
