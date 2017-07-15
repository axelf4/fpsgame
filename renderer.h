#ifndef RENDERER_H
#define RENDERER_H

#include <GL/glew.h>
#include <vmath.h>
#include "model.h"

// The number of cascades.
#define NUM_SPLITS 3

struct ModelInstance {
	struct Model *model;
};

struct Renderer {
	MATRIX model, view, projection, prevViewProjection;
	GLuint program;
	GLint posAttrib, normalAttrib, mvpUniform, modelUniform;

	GLuint sceneFbo, sceneTexture;

	GLuint depthProgram, depthFbo,
		   depthTexture, shadowMaps[NUM_SPLITS]; // Depth textures
	GLint depthProgramPosition, depthProgramMvp;

	GLuint quadBuffer,
		   ssaoProgram, ssaoTexture, ssaoFbo,
		   blur1Program, blur2Program, blurTexture, blurFbo,
		   motionBlurProgram;

	GLuint skyboxTexture, skyboxProgram;
	GLint skyboxPositionAttrib;

	struct ModelInstance instances[64];
};

int rendererInit(struct Renderer *renderer);

void rendererDestroy(struct Renderer *renderer);

void rendererDraw(struct Renderer *renderer, VECTOR position, float yaw, float pitch, float dt);

#endif
