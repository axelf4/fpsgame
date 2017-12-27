#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "state.h"
#include "spriteBatch.h"
#include "entity.h"
#include "renderer.h"
#include "model.h"
#include "font.h"
#include "widget.h"
#include "label.h"

struct PlayerData {
	float turn;
	int dead : 1;
	float deadTimer;
};

struct InGameUI {
	struct Label scoreLabel;
};

struct GameOverUI {
	struct Label label;
};

typedef struct GameState {
	struct State state;
	struct SpriteBatch *batch;
	struct Font *font;
	struct EntityManager manager;
	struct Renderer renderer;

	VECTOR position;
	float yaw, pitch;
	struct Model *objModel;
	struct Model *groundModel;

	struct GuiContext context;

	struct InGameUI inGameUI;
	struct GameOverUI gameOverUI;

	struct Widget *flexLayout, *image0, *image1, *label;
	GLuint cat;

	int noclip : 1;
	struct PlayerData playerData;

	Entity player;
} GameState;

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch, struct Font *font);

void gameStateDestroy(struct GameState *gameState);

#endif
