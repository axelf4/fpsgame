#ifndef MODEL_H
#define MODEL_H

#include <GL/glew.h>

struct Material {
	float diffuse[3];
};

typedef struct ModelPart {
	unsigned int count, offset;
	int materialIndex;
} ModelPart;

typedef struct Model {
	GLuint vertexBuffer, indexBuffer;
	size_t indexCount;
	GLsizei stride;
	int numParts;
	struct ModelPart *parts;
	struct Material *materials;
	float radius;
} Model;

struct Model *loadModelFromObj(char *path);

void destroyModel(struct Model *model);

#endif
