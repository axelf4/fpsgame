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

typedef struct GameState {
	struct State state;
	struct SpriteBatch *batch;

	GLuint program;
	GLint posAttrib, modelUniform, viewUniform, projectionUniform;
	MATRIX model, view, projection;

	GLuint skyboxTexture;
	GLuint skyboxProgram;
	GLuint skyboxVertexBuffer, skyboxIndexBuffer;

	VECTOR position;
	float yaw, pitch;
	struct Model *objModel;
	struct Model *groundModel;

	GLuint depthProgram, depthFbo,
		   // Depth textures
		   shadowMaps[NUM_SPLITS];

	struct Font *font;
	struct Widget *flexLayout, *image0, *image1, *label;
	GLuint cat;
} GameState;

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch);

void gameStateDestroy(struct GameState *gameState);

#endif
