#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "state.h"
#include <GL/glew.h>
#include "spriteBatch.h"
#include "model.h"
#include "font.h"
#include "widget.h"

// The number of cascades.
#define NUM_SPLITS 3

#define SAO_MAX_MIP_LEVEL 5

typedef struct GameState {
	struct State state;
	struct SpriteBatch *batch;

	GLuint program;
	GLint posAttrib, normalAttrib, modelUniform, viewUniform, projectionUniform;
	MATRIX model, view, projection;

	GLuint skyboxTexture;
	GLuint skyboxProgram;
	GLuint skyboxVertexBuffer, skyboxIndexBuffer;

	VECTOR position;
	float yaw, pitch;
	struct Model *objModel;
	struct Model *groundModel;

	GLuint depthProgram, depthProgramPosition, depthProgramMvp,
		   depthFbo, shadowMaps[NUM_SPLITS]; // Depth textures

	GLuint depthTexture;
	GLuint ssaoFbo, ssaoTexture, ssaoProgram;
	GLuint quadBuffer;
	GLuint reconstructCSZProgram, minifyProgram;
	GLuint cszTexture;
	GLuint cszFramebuffers[SAO_MAX_MIP_LEVEL];
	GLuint blurProgram, blurTexture, blurFbo, saoResultFbo;

	struct Font *font;
	struct Widget *flexLayout, *image0, *image1, *label;
	GLuint cat;
} GameState;

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch);

void gameStateDestroy(struct GameState *gameState);

#endif
