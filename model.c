#include "model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <objparser.h>
#include "glUtil.h"

struct ObjGroup {
	int faceIndex;
	int materialIndex;
	struct ObjGroup *next;
};

struct ObjBuilder {
	size_t verticesSize, verticesCapacity, texcoordsSize, texcoordsCapacity, normalsSize, normalsCapacity, indicesSize, indicesCapacity,
		   numMaterials;
	unsigned int numFaces;
	float *vertices,
		  *texcoords,
		  *normals;
	struct ObjVertexIndex *indices;
	struct ObjGroup *groupHead, *currentGroup;
	struct MtlMaterial *materials;
	char *path;
};

static void addVertexCB(void *prv, float x, float y, float z, float w) {
	struct ObjBuilder *obj = prv;
	if (obj->verticesSize + 3 > obj->verticesCapacity) {
		float *tmp = realloc(obj->vertices, sizeof(float) * (obj->verticesCapacity *= 2));
		assert(tmp && "Failed to reallocate array.");
		obj->vertices = tmp;
	}
	obj->vertices[obj->verticesSize++] = x;
	obj->vertices[obj->verticesSize++] = y;
	obj->vertices[obj->verticesSize++] = z;
}

static void addTexcoordCB(void *prv, float x, float y, float z) {
	struct ObjBuilder *obj = prv;
	if (obj->texcoordsSize + 3 > obj->texcoordsCapacity) {
		float *tmp = realloc(obj->texcoords, sizeof(float) * (obj->texcoordsCapacity *= 2));
		assert(tmp && "Failed to reallocate array.");
		obj->texcoords = tmp;
	}
	obj->texcoords[obj->texcoordsSize++] = x;
	obj->texcoords[obj->texcoordsSize++] = y;
	obj->texcoords[obj->texcoordsSize++] = z;
}

static void addNormalCB(void *prv, float x, float y, float z) {
	struct ObjBuilder *obj = prv;
	if (obj->normalsSize + 3 > obj->normalsCapacity) {
		float *tmp = realloc(obj->normals, sizeof(float) * (obj->normalsCapacity *= 2));
		assert(tmp && "Failed to reallocate array.");
		obj->normals = tmp;
	}
	obj->normals[obj->normalsSize++] = x;
	obj->normals[obj->normalsSize++] = y;
	obj->normals[obj->normalsSize++] = z;
}

static void addFaceCB(void *prv, int numVertices, struct ObjVertexIndex *indices) {
	struct ObjBuilder *obj = prv;
	if (obj->indicesSize + numVertices > obj->indicesCapacity) {
		struct ObjVertexIndex *tmp = realloc(obj->indices, sizeof(struct ObjVertexIndex) * (obj->indicesCapacity *= 2));
		assert(tmp && "Failed to reallocate array.");
		obj->indices = tmp;
	}
	for (int i = 0; i < numVertices; ++i) {
		obj->indices[obj->indicesSize++] = indices[i];
	}
	++obj->numFaces;
}

static void pushGroup(struct ObjBuilder *obj, int materialIndex) {
	struct ObjGroup *group = malloc(sizeof(struct ObjGroup));
	assert(group);
	group->materialIndex = materialIndex;
	group->faceIndex = obj->numFaces;
	group->next = 0;

	// Add it into list
	*(obj->currentGroup ? &obj->currentGroup->next : &obj->groupHead) = group;
	obj->currentGroup = group;
}

static void addGroupCB(void *prv, int numNames, char **names) {
	struct ObjBuilder *obj = prv;
	int materialIndex = obj->currentGroup ? obj->currentGroup->materialIndex : -1;
	pushGroup(obj, materialIndex);
}

static void mtllib(void *prv, char *path) {
	struct ObjBuilder *obj = prv;
	char *data;

	char *lastSlash = strrchr(obj->path, '/');
	if (lastSlash) {
		int numChars = lastSlash - obj->path, len = strlen(path);
		char *combinedPath = malloc(sizeof(char) * (numChars + 1 + len));
		assert(combinedPath && "Failed to allocate memory.");
		memcpy(combinedPath, obj->path, numChars);
		combinedPath[numChars] = '/';
		memcpy(combinedPath + numChars + 1, path, len);
		data = readFile(combinedPath);
		free(combinedPath);
	} else data = readFile(path);

	assert(data && "Failed to load file.");
	unsigned int numMaterials;
	struct MtlMaterial *loadedMaterials = loadMtl(data, &numMaterials, 0);
	free(data);

	struct MtlMaterial *tmp = realloc(obj->materials, sizeof(struct MtlMaterial) * (obj->numMaterials + numMaterials));
	assert(tmp);
	obj->materials = tmp;
	memcpy(obj->materials + obj->numMaterials, loadedMaterials, sizeof(struct MtlMaterial) * numMaterials);
	obj->numMaterials += numMaterials;
}

static void usemtl(void *prv, char *name) {
	struct ObjBuilder *obj = prv;
	int materialIndex = -1;
	for (int i = 0; i < obj->numMaterials; ++i) {
		struct MtlMaterial *material = obj->materials + i;
		if (strcmp(name, material->name) == 0) {
			materialIndex = i;
			break;
		}
	}
	assert(materialIndex >= 0 && "Unknown material.");
	pushGroup(obj, materialIndex);
}

static void *mallocCB(size_t size) {
	return malloc(size);
}

static void freeCB(void *ptr) {
	free(ptr);
}

struct Model *loadModelFromObj(char *path) {
	printf("loading model: %s\n", path);
	char *buffer = readFile(path);
	if (!buffer) {
		fprintf(stderr, "Error loading file: %s.\n", path);
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
	obj.numMaterials = 0;
	obj.vertices = malloc(sizeof(float) * obj.verticesCapacity);
	obj.texcoords = malloc(sizeof(float) * obj.texcoordsCapacity);
	obj.normals = malloc(sizeof(float) * obj.normalsCapacity);
	obj.indices = malloc(sizeof(struct ObjVertexIndex) * obj.indicesCapacity);
	obj.materials = 0;
	obj.currentGroup = obj.groupHead = 0;
	obj.path = path;
	struct ObjParserContext context = { &obj, addVertexCB, addTexcoordCB, addNormalCB, addFaceCB, addGroupCB, mtllib, usemtl, mallocCB, freeCB, OBJ_TRIANGULATE };
	objParse(&context, buffer);

	unsigned int vertexCount = 0, indexCount = 0;
	// Assume that each face is a triangle
	GLfloat *vertices = malloc(sizeof(float) * obj.numFaces * 3 * (3 + 2 * obj.texcoordsSize + 3 * obj.normalsSize));
	unsigned int *indices = malloc(sizeof(unsigned int) * obj.numFaces * 3);
	for (unsigned face = 0, k = 0; face < obj.numFaces; ++face) {
		for (unsigned i = 0; i < 3; ++i, ++indexCount) {
			struct ObjVertexIndex vi = obj.indices[indexCount];
			unsigned int vertexIndex = vi.vertexIndex * 3, texcoordIndex = vi.texcoordIndex * 2, normalIndex = vi.normalIndex * 3;

			for (int j = 0; j < indexCount; ++j) {
				struct ObjVertexIndex vi2 = obj.indices[j];
				if (vi.vertexIndex == vi2.vertexIndex && vi.texcoordIndex == vi2.texcoordIndex && vi.normalIndex == vi2.normalIndex) {
					indices[indexCount] = indices[j];
					goto existingVertex;
				}
			}

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
existingVertex:;
		}
	}
	free(buffer);
	printf("numFaces: %d, vertexCount: %d, indexCount: %d.\n", obj.numFaces, vertexCount, indexCount);

	struct Model *model = malloc(sizeof(struct Model));
	model->stride = sizeof(GLfloat) * (3 + 2 * !!obj.texcoordsSize + 3 * !!obj.normalsSize);
	model->indexCount = indexCount;
	// Copy geometry to the GPU
	glGenBuffers(1, &model->vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vertexCount, vertices, GL_STATIC_DRAW);
	free(vertices);
	glGenBuffers(1, &model->indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indexCount, indices, GL_STATIC_DRAW);
	free(indices);

	struct ObjGroup defaultGroup = { 0, -1, 0 };
	if (!obj.groupHead) obj.groupHead = &defaultGroup;

	// Count the number of parts
	int numParts = 0;
	struct ObjGroup *group = obj.groupHead;
	do ++numParts; while ((group = group->next));
	struct ModelPart *parts = malloc(sizeof(struct ModelPart) * numParts);
	group = obj.groupHead;
	for (int i = 0; i < numParts; ++i) {
		struct ModelPart *part = parts + i;
		part->offset = group->faceIndex * 3;
		part->count = ((group->next ? group->next->faceIndex : obj.numFaces) - group->faceIndex) * 3;
		part->materialIndex = group->materialIndex;

		group = group->next;
	}
	model->numParts = numParts;
	model->parts = parts;

	model->materials = malloc(sizeof(struct Material) * obj.numMaterials);
	for (int i = 0; i < obj.numMaterials; ++i) {
		struct MtlMaterial *mtl = obj.materials + i;
		struct Material *material = model->materials + i;
		material->diffuse[0] = mtl->diffuse[0];
		material->diffuse[1] = mtl->diffuse[1];
		material->diffuse[2] = mtl->diffuse[2];
	}

	return model;
}

void destroyModel(struct Model *model) {
	GLuint buffers[] = { model->vertexBuffer, model->indexBuffer };
	glDeleteBuffers(2, buffers);
	free(model);
}
