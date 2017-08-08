#include "gameState.h"
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include "pngloader.h"
#include <float.h>
#include <stdint.h>
#include <GL/glew.h>

#include "image.h"
#include "label.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .02f
#define TURNING_TIME 1500.0f

static float getTurningFactor(float turn) {
	float x = fabs(turn) / TURNING_TIME;

	x = sin(0.5f * M_PI * x);

	if (turn < 0) x = -x;
	return x;
}

static void gameStateUpdate(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	struct EntityManager *manager = &gameState->manager;
	const Uint8 *keys = SDL_GetKeyboardState(NULL);

	if (gameState->noclip) {
		int x, y;
		Uint32 button = SDL_GetRelativeMouseState(&x, &y);
		gameState->yaw -= x * MOUSE_SENSITIVITY;
		gameState->pitch -= y * MOUSE_SENSITIVITY;
	} else {
		if (keys[SDL_SCANCODE_A] ^ keys[SDL_SCANCODE_D]) {
			gameState->playerData.turn += keys[SDL_SCANCODE_A] ? -dt : dt;
		} else {
			if (fabs(gameState->playerData.turn) < TURNING_TIME / 3.0f) {
				if (gameState->playerData.turn < -dt) gameState->playerData.turn += dt;
				else if (gameState->playerData.turn > dt) gameState->playerData.turn -= dt;
				else gameState->playerData.turn = 0;
			}
		}

		if (gameState->playerData.turn < -TURNING_TIME) gameState->playerData.turn = -TURNING_TIME;
		else if (gameState->playerData.turn > TURNING_TIME) gameState->playerData.turn = TURNING_TIME;

		const float rotation = 0.08f * getTurningFactor(gameState->playerData.turn);
		gameState->yaw -= rotation;
	}

	if (gameState->yaw > M_PI) gameState->yaw -= 2 * M_PI;
	else if (gameState->yaw < -M_PI) gameState->yaw += 2 * M_PI;
	// Clamp the pitch
	gameState->pitch = gameState->pitch < -M_PI / 2 ? -M_PI / 2 : gameState->pitch > M_PI / 2 ? M_PI / 2 : gameState->pitch;

	VECTOR forward = VectorSet(-MOVEMENT_SPEED * sin(gameState->yaw) * dt, 0, -MOVEMENT_SPEED * cos(gameState->yaw) * dt, 0),
		   up = VectorSet(0, 1, 0, 0),
		   right = VectorCross(forward, up);

	if (gameState->noclip) {
		VECTOR position = gameState->position;
		if (keys[SDL_SCANCODE_W]) position = VectorAdd(position, forward);
		if (keys[SDL_SCANCODE_A]) position = VectorSubtract(position, right);
		if (keys[SDL_SCANCODE_S]) position = VectorSubtract(position, forward);
		if (keys[SDL_SCANCODE_D]) position = VectorAdd(position, right);
		if (keys[SDL_SCANCODE_SPACE]) position = VectorAdd(position, VectorSet(0, MOVEMENT_SPEED * dt, 0, 0));
		if (keys[SDL_SCANCODE_LSHIFT]) position = VectorSubtract(position, VectorSet(0, MOVEMENT_SPEED * dt, 0, 0));
		gameState->position = position;
	} else {
		VECTOR *position = &manager->positions[gameState->player].position;
		if (keys[SDL_SCANCODE_W]) *position = VectorAdd(*position, forward);
	}
}

static void gameStateDraw(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	struct SpriteBatch *batch = gameState->batch;
	struct EntityManager *manager = &gameState->manager;

	VECTOR position = gameState->noclip ? gameState->position : VectorAdd(manager->positions[gameState->player].position, VectorSet(0.0f, 1.4f, 0.0f, 0.0f));
	const float roll = M_PI / 8 * getTurningFactor(gameState->playerData.turn);

	rendererDraw(&gameState->renderer, position, gameState->yaw, gameState->pitch, roll, dt);

	// Draw GUI
	spriteBatchBegin(batch);
	// widgetValidate(gameState->flexLayout, 800, 600);
	// widgetDraw(gameState->flexLayout, batch);
	spriteBatchEnd(batch);
}

static void gameStateResize(struct State *state, int width, int height) {
	struct GameState *gameState = (struct GameState *) state;
	rendererResize(&gameState->renderer, width, height);
	widgetLayout(gameState->flexLayout, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);
}

static struct FlexParams params0 = { ALIGN_END, -1, 100, UNDEFINED, 20, 0, 20, 20 },
						 params2 = {ALIGN_CENTER, 1, 100, UNDEFINED, 0, 0, 0, 50},
						 params1 = { ALIGN_CENTER, 1, UNDEFINED, UNDEFINED, 0, 0, 0, 0 };

/**
 * Returns a random float between 0 and 1.
 */
static float randomFloat() {
	return rand() / RAND_MAX;
}

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch) {
	struct State *state = (struct State *) gameState;
	state->update = gameStateUpdate;
	state->draw = gameStateDraw;
	state->resize = gameStateResize;
	gameState->batch = batch;
	struct EntityManager *manager = &gameState->manager;
	entityManagerInit(manager);
	struct Renderer *renderer = &gameState->renderer;
	rendererInit(renderer, manager, 800, 600);

	gameState->position = VectorSet(0, 0, 0, 1);
	gameState->yaw = 0;
	gameState->pitch = 0;
	gameState->objModel = loadModelFromObj("pyramid.obj");
	if (!gameState->objModel) {
		printf("Failed to load model.\n");
	}
	gameState->groundModel = loadModelFromObj("ground.obj");
	if (!gameState->groundModel) {
		printf("Failed to load ground model.\n");
	}

	gameState->player = entityManagerSpawn(manager);
	manager->entityMasks[gameState->player] = POSITION_COMPONENT_MASK;
	manager->positions[gameState->player].position = VectorSet(0, 0, 0, 1);

	Entity ground = entityManagerSpawn(manager);
	manager->entityMasks[ground] = POSITION_COMPONENT_MASK | MODEL_COMPONENT_MASK;
	manager->positions[ground].position = VectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	manager->models[ground].model = gameState->groundModel;

	Entity enemy = entityManagerSpawn(manager);
	manager->entityMasks[enemy] = POSITION_COMPONENT_MASK | MODEL_COMPONENT_MASK;
	manager->positions[enemy].position = VectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	manager->models[enemy].model = gameState->objModel;

	// Initialize GUI
	gameState->flexLayout = malloc(sizeof(struct FlexLayout));
	flexLayoutInitialize(gameState->flexLayout, DIRECTION_ROW, ALIGN_START);

	png_uint_32 width, height;
	gameState->cat = loadPNGTexture("cat.png", &width, &height);
	if (!gameState->cat) {
		fprintf(stderr, "Failed to load png image.\n");
	}
	gameState->image0 = malloc(sizeof(struct Image));
	imageInitialize(gameState->image0, gameState->cat, width, height, 0);
	gameState->image0->layoutParams = &params0;
	containerAddChild(gameState->flexLayout, gameState->image0);

	gameState->image1 = malloc(sizeof(struct Image));
	imageInitialize(gameState->image1, gameState->cat, width, height, 0);
	gameState->image1->layoutParams = &params1;
	containerAddChild(gameState->flexLayout, gameState->image1);

	gameState->font = loadFont("DejaVuSans.ttf", 512, 512);
	if (!gameState->font) {
		printf("Could not load font.");
	}

	gameState->label = labelNew(gameState->font, "Axel ffi! and the AV. HHHHHHHH Hi! (215): tv-hund. fesflhslg");
	gameState->label->layoutParams = &params2;
	containerAddChild(gameState->flexLayout, gameState->label);

	gameState->noclip = 0;

	gameState->playerData.turn = 0.0f;
}

void gameStateDestroy(struct GameState *gameState) {
	rendererDestroy(&gameState->renderer);
	destroyModel(gameState->objModel);

	fontDestroy(gameState->font);
	containerDestroy(gameState->flexLayout);
	free(gameState->flexLayout);
	free(gameState->image0);
	free(gameState->image1);
	labelDestroy(gameState->label);
	free(gameState->label);
	glDeleteTextures(1, &gameState->cat);
}
