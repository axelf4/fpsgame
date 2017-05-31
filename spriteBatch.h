#ifndef RENDERER_H
#define RENDERER_H

#include <GL/glew.h>
#include <vmath.h>
#include "font.h"

#include "linebreak.h"

typedef struct Color {
	float r, g, b, a;
} Color;

struct SpriteBatch {
	GLuint vertexObject, indexObject;
	// Number of sprites able to be stored
	int index;
	int maxVertices;
	/** Whether or not we are drawing. */
	int drawing;
	float *vertices;
	GLuint program, defaultProgram;
	MATRIX projectionMatrix;
	GLint vertexAttrib, texCoordAttrib;
	GLuint lastTexture;

	GLuint textProgram;
};

void spriteBatchInitialize(struct SpriteBatch *batch, int size);

void spriteBatchDestroy(struct SpriteBatch *batch);

void spriteBatchBegin(struct SpriteBatch *batch);

void spriteBatchEnd(struct SpriteBatch *batch);

void spriteBatchSwitchProgram(struct SpriteBatch *batch, GLuint program);

void spriteBatchDraw(struct SpriteBatch *batch, GLuint texture, float x, float y, float width, float height);

void spriteBatchDrawCustom(struct SpriteBatch *batch, GLuint texture, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1);

void spriteBatchDrawLayout(struct SpriteBatch *batch, struct Layout *layout, Color color, float x, float y);

#endif
