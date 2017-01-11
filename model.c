#include "model.h"
#include <stdlib.h>
#include <stdio.h>
#include <objparser.h>

char *readFile(const char *filename) {
	printf("Loading file %s.\n", filename);
	char *buffer = 0;
	FILE *f = fopen(filename, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		long length = ftell(f);
		fseek(f, 0, SEEK_SET);
		if ((buffer = malloc(length + 1)) == 0) return 0;
		buffer[length] = 0;
		if (buffer) fread(buffer, 1, length, f);
		fclose(f);
	}
	return buffer;
}

void addVertexCB(void *prv, float x, float y, float z, float w) {
	struct ObjBuilder *obj = prv;
	if (obj->verticesSize + 3 >= obj->verticesCapacity) {
		float *tmp = realloc(obj->vertices, sizeof(float) * (obj->verticesCapacity *= 2));
		// if (!tmp) goto error;
		obj->vertices = tmp;
	}
	obj->vertices[obj->verticesSize++] = x;
	obj->vertices[obj->verticesSize++] = y;
	obj->vertices[obj->verticesSize++] = z;
}

void addTexcoordCB(void *prv, float x, float y, float z) {
	struct ObjBuilder *obj = prv;
	if (obj->texcoordsSize + 3 >= obj->texcoordsCapacity) {
		float *tmp = realloc(obj->texcoords, sizeof(float) * (obj->texcoordsCapacity *= 2));
		// if (!tmp) goto error;
		obj->texcoords = tmp;
	}
	obj->texcoords[obj->texcoordsSize++] = x;
	obj->texcoords[obj->texcoordsSize++] = y;
	obj->texcoords[obj->texcoordsSize++] = z;
}

void addNormalCB(void *prv, float x, float y, float z) {
	struct ObjBuilder *obj = prv;
	if (obj->normalsSize + 3 >= obj->normalsCapacity) {
		float *tmp = realloc(obj->normals, sizeof(float) * (obj->normalsCapacity *= 2));
		// if (!tmp) goto error;
		obj->normals = tmp;
	}
	obj->normals[obj->normalsSize++] = x;
	obj->normals[obj->normalsSize++] = y;
	obj->normals[obj->normalsSize++] = z;
}

void addFaceCB(void *prv, int numVertices, struct ObjVertexIndex *indices) {
	struct ObjBuilder *obj = prv;
	if (obj->indicesSize + 3 >= obj->indicesCapacity) {
		struct ObjVertexIndex *tmp = realloc(obj->indices, sizeof(struct ObjVertexIndex) * (obj->indicesCapacity *= 2));
		// if (!tmp) goto error;
		obj->indices = tmp;
	}
	for (int i = 0; i < numVertices; ++i) {
		obj->indices[obj->indicesSize++] = indices[i];
	}
	++obj->numFaces;
}

void addGroupCB(void *prv, int numNames, char **names) {
	struct ObjBuilder *obj = prv;
	printf("group name: %s.\n", *names);
}
void mtllib(void *prv, char *path) {}
void usemtl(void *prv, char *name) {}

void *mallocCB(size_t size) {
	return malloc(size);
}

void freeCB(void *ptr) {
	free(ptr);
}

struct Model *loadModelFromObj(const char *path) {
	printf("loading model: %s\n", path);
	char *buffer = readFile(path);
	if (!buffer) {
		printf("Error loading file: %s.\n", path);
		return 0;
	}
	struct ObjBuilder obj;
	obj.verticesSize = 0;
	obj.verticesCapacity = 3;
	obj.texcoordsSize = 0;
	obj.texcoordsCapacity = 2;
	obj.normalsSize = 0;
	obj.normalsCapacity = 3;
	obj.indicesSize = 0;
	obj.indicesCapacity = 2;
	obj.numFaces = 0;
	obj.vertices = malloc(sizeof(float) * obj.verticesCapacity);
	obj.texcoords = malloc(sizeof(float) * obj.texcoordsCapacity);
	obj.normals = malloc(sizeof(float) * obj.normalsCapacity);
	obj.indices = malloc(sizeof(struct ObjVertexIndex) * obj.indicesCapacity);
	struct ObjParserContext context = { &obj, addVertexCB, addTexcoordCB, addNormalCB, addFaceCB, addGroupCB, 0, 0, mallocCB, freeCB, OBJ_TRIANGULATE };
	objParse(&context, buffer);

	unsigned int vertexCount, indexCount = 0;
	GLfloat *vertices;
	unsigned int *indices;
	// Assume that each face is a triangle
	vertices = malloc(sizeof(float) * obj.numFaces * 3 * (3 + 2 * obj.texcoordsSize + 3 * obj.normalsSize));
	indices = malloc(sizeof(unsigned int) * obj.numFaces * 3);
	unsigned k = 0;
	for (unsigned face = 0; face < obj.numFaces; ++face) {
		for (unsigned i = 0; i < 3; ++i) {
			struct ObjVertexIndex vi = obj.indices[indexCount];
			unsigned int vertexIndex = vi.vertexIndex * 3, texcoordIndex = vi.texcoordIndex * 2, normalIndex = vi.normalIndex * 3;
			int existingVertex = 0;

			for (int j = 0; j < indexCount; ++j) {
				struct ObjVertexIndex vi2 = obj.indices[j];
				if (vi.vertexIndex == vi2.vertexIndex && vi.texcoordIndex == vi2.texcoordIndex && vi.normalIndex == vi2.normalIndex) {
					indices[indexCount] = indices[j];
					existingVertex = 1;
					break;
				}
			}

			if (!existingVertex) {
				indices[indexCount] = k++;
				vertices[vertexCount++] = obj.vertices[vertexIndex];
				vertices[vertexCount++] = obj.vertices[vertexIndex + 1];
				vertices[vertexCount++] = obj.vertices[vertexIndex + 2];
				if (vi.texcoordIndex != -1) {
					vertices[vertexCount++] = obj.vertices[texcoordIndex];
					vertices[vertexCount++] = obj.vertices[texcoordIndex + 1];
				}
				if (vi.normalIndex != -1) {
					vertices[vertexCount++] = obj.normals[normalIndex];
					vertices[vertexCount++] = obj.normals[normalIndex + 1];
					vertices[vertexCount++] = obj.normals[normalIndex + 2];
				}
			}

			++indexCount;
		}
	}
	printf("vertexCount: %d, indexCount: %d.\n", vertexCount, indexCount);
	free(buffer);

	// Create a Vertex Buffer Object and copy the vertex data to it
	GLuint vertexBuffer;
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
	GLuint indexBuffer;
	glGenBuffers(1, &indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indexCount, indices, GL_STATIC_DRAW);

	struct Model *model = malloc(sizeof(struct Model));
	model->vertexBuffer = vertexBuffer;
	model->indexBuffer = indexBuffer;
	model->indexCount = indexCount;
	return model;
}

void destroyModel(struct Model *model) {
	glDeleteBuffers(1, &model->vertexBuffer);
	glDeleteBuffers(1, &model->indexBuffer);
	free(model);
}
