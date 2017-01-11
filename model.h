#ifndef MODEL_H
#define MODEL_H

#include <gl/glew.h>

typedef struct Model {
	GLuint vertexBuffer, indexBuffer;
	size_t indexCount;
} Model;

typedef struct ObjBuilder {
	size_t verticesSize, verticesCapacity, texcoordsSize, texcoordsCapacity, normalsSize, normalsCapacity, indicesSize, indicesCapacity;
	unsigned int numFaces;
	float *vertices,
		  *texcoords,
		  *normals;
	struct ObjVertexIndex *indices;
} ObjBuilder;

struct Model *loadModelFromObj(const char *path);

void destroyModel(struct Model *model);

#endif
