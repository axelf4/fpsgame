#ifndef RENDERER_H
#define RENDERER_H

#include <GL/glew.h>
#include <vmath.h>
#include "entity.h"
#include "model.h"

// The number of cascades.
#define NUM_SPLITS 3

struct Renderer {
	struct EntityManager *manager;
	int width, height;
	MATRIX model, view, projection, prevViewProjection;
	GLuint program;
	GLint posAttrib, normalAttrib, mvpUniform, modelUniform, colorUniform;

	GLuint sceneFbo, sceneTexture;

	GLuint depthProgram, depthFbo,
		   depthTexture, shadowMaps[NUM_SPLITS]; // Depth textures
	GLint depthProgramPosition, depthProgramMvp;

	GLuint quadBuffer,
		   ssaoProgram, ssaoPosition, ssaoTexture, ssaoFbo,
		   blur1Program, blur1Position, blur2Program, blur2Position, blurTexture, blurFbo,
		   effectProgram, effectPosition, effectCurrToPrevUniform, effectBlurFactorUniform, effectFactorUniform;

	GLuint skyboxTexture, skyboxProgram;
	GLint skyboxPositionAttrib;
};

int rendererInit(struct Renderer *renderer, struct EntityManager *manager, int width, int height);

void rendererResize(struct Renderer *renderer, int width, int height);

void rendererDestroy(struct Renderer *renderer);

void rendererDraw(struct Renderer *renderer, VECTOR position, float yaw, float pitch, float roll, float dt);

void rendererSetEffectFactor(struct Renderer *renderer, float f);

#endif
