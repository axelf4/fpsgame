#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "state.h"
#include "spriteBatch.h"
#include "entity.h"
#include "renderer.h"
#include "model.h"
#include "font.h"
#include "widget.h"

typedef struct GameState {
	struct State state;
	struct SpriteBatch *batch;
	struct EntityManager manager;
	struct Renderer renderer;

	VECTOR position;
	float yaw, pitch;
	struct Model *objModel;
	struct Model *groundModel;

	struct Font *font;
	struct Widget *flexLayout, *image0, *image1, *label;
	GLuint cat;
} GameState;

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch);

void gameStateDestroy(struct GameState *gameState);

#endif
