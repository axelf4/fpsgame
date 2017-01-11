#ifndef SPRITEBATCH_H
#define SPRITEBATCH_H

#include <GL/glew.h>
#include <vmath.h>

typedef struct SpriteBatch {
	GLuint program;
	float *vertices;
	int idx, size;
	MATRIX projectionMatrix;
} SpriteBatch;

void drawSpriteBatch(struct SpriteBatch);

#endif
