#include "spriteBatch.h"
#include <stdlib.h>

#define SPRITE_SIZE 16

void initSpriteBatch(struct SpriteBatch *batch) {
	int capacity = 800;
	batch->size = capacity * SPRITE_SIZE;
	batch->idx = 0;

	batch->vertices = malloc(sizeof(float) * batch->size);
	batch->vertexBuffer = glCreateBuffer();

	size_t indexCount = capacity * 6;
	unsigned short *indices = malloc(sizeof(unsigned short) * indexCount);
	for (size_t i = 0, j = 0; i < indexCount; i += 6, j += 4) {
		indices[i] = j;
		indices[i + 1] = j + 1;
		indices[i + 2] = j + 2;
		indices[i + 3] = j;
		indices[i + 4] = j + 2;
		indices[i + 5] = j + 3;
	}
	batch->indexBuffer = glCreateBuffer();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices, GL_STATIC_DRAW);

	const char vertexShaderSource[] = "attribute vec2 aVertexPosition;"
		"attribute vec2 aTextureCoord;"

		"uniform mat4 uMVMatrix;"
		"uniform mat4 uPMatrix;"

		"varying highp vec2 vTextureCoord;"

		"void main(void) {"
		"	gl_Position = uPMatrix * uMVMatrix * vec4(aVertexPosition, 0.0, 1.0);"
		"	vTextureCoord = aTextureCoord;"
		"}";
	const char fragmentShaderSource[] = "precision mediump float;"
		"varying highp vec2 vTextureCoord;"
		"uniform sampler2D uSampler;"

		"void main(void) {"
		"	gl_FragColor = texture2D(uSampler, vTextureCoord);"
		"}";
}
