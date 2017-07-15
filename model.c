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
};

static void addVertexCB(void *prv, float x, float y, float z, float w) {
	struct ObjBuilder *obj = prv;
	if (obj->verticesSize + 3 > obj->verticesCapacity) {
		float *tmp = realloc(obj->vertices, sizeof(float) * (obj->verticesCapacity *= 2));
		// if (!tmp) goto error;
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
		// if (!tmp) goto error;
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
		// if (!tmp) goto error;
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
		// if (!tmp) goto error;
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
	/*struct ObjGroup **headPtr = &obj->groupHead;
	while (*headPtr)
		headPtr = &(*headPtr)->next;
	*headPtr = group;*/
	*(obj->currentGroup ? &obj->currentGroup->next : &obj->groupHead) = group;
	obj->currentGroup = group;
}

static void addGroupCB(void *prv, int numNames, char **names) {
	struct ObjBuilder *obj = prv;
	printf("group name: %s.\n", *names);
	int materialIndex = obj->currentGroup ? obj->currentGroup->materialIndex : -1;
	pushGroup(obj, materialIndex);

}

static void mtllib(void *prv, char *path) {
	struct ObjBuilder *obj = prv;
	char *data = readFile(path);
	assert(data && "Failed to load file.");
	unsigned int numMaterials;
	struct MtlMaterial *loadedMaterials = loadMtl(data, &numMaterials, 0);
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
	obj.numMaterials = 0;
	obj.vertices = malloc(sizeof(float) * obj.verticesCapacity);
	obj.texcoords = malloc(sizeof(float) * obj.texcoordsCapacity);
	obj.normals = malloc(sizeof(float) * obj.normalsCapacity);
	obj.indices = malloc(sizeof(struct ObjVertexIndex) * obj.indicesCapacity);
	obj.materials = 0;
	obj.currentGroup = obj.groupHead = 0;
	struct ObjParserContext context = { &obj, addVertexCB, addTexcoordCB, addNormalCB, addFaceCB, addGroupCB, mtllib, usemtl, mallocCB, freeCB, OBJ_TRIANGULATE };
	objParse(&context, buffer);

	printf("numFaces: %d\n", obj.numFaces);

	unsigned int vertexCount = 0, indexCount = 0;
	// Assume that each face is a triangle
	GLfloat *vertices = malloc(sizeof(float) * obj.numFaces * 3 * (3 + 2 * obj.texcoordsSize + 3 * obj.normalsSize));
	unsigned int *indices = malloc(sizeof(unsigned int) * obj.numFaces * 3);
	for (unsigned face = 0, k = 0; face < obj.numFaces; ++face) {
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
	model->stride = sizeof(GLfloat) * (3 + 3 * !!obj.normalsSize + 2 * !!obj.texcoordsSize);
	printf("model->stride: %d\n", model->stride);

	struct ObjGroup defaultGroup = { 0, -1, 0 };
	if (!obj.groupHead)
		obj.groupHead = &defaultGroup;

	int numParts = 0;
	struct ObjGroup *group = obj.groupHead;
	do ++numParts; while (group = group->next);
	struct ModelPart *parts = malloc(sizeof(struct ModelPart) * numParts);
	group = obj.groupHead;
	for (int i = 0; i < numParts; ++i) {
		struct ModelPart *part = parts + i;
		part->offset = group->faceIndex * 3;
		part->count = ((group->next ? group->next->faceIndex : obj.numFaces) - group->faceIndex) * 3;
		printf("part->offset: %d\n", part->offset);
		printf("part->count: %d\n", part->count);
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

	model->indexCount = indexCount;
	return model;
}

void destroyModel(struct Model *model) {
	glDeleteBuffers(1, &model->vertexBuffer);
	glDeleteBuffers(1, &model->indexBuffer);
	free(model);
}
