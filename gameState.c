#include "gameState.h"
#include <stdio.h>
#include <SDL.h>
#include "pngloader.h"
#include <float.h>
#include <stdint.h>
#include <GL/glew.h>

#include "image.h"
#include "label.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .02f

static void gameStateUpdate(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	int x, y;
	Uint32 button = SDL_GetRelativeMouseState(&x, &y);
	gameState->yaw -= x * MOUSE_SENSITIVITY;
	gameState->pitch -= y * MOUSE_SENSITIVITY;
	if (gameState->yaw > M_PI) gameState->yaw -= 2 * M_PI;
	else if (gameState->yaw < -M_PI) gameState->yaw += 2 * M_PI;
	// Clamp the pitch
	gameState->pitch = gameState->pitch < -M_PI / 2 ? -M_PI / 2 : gameState->pitch > M_PI / 2 ? M_PI / 2 : gameState->pitch;
	VECTOR forward = VectorSet(-MOVEMENT_SPEED * sin(gameState->yaw) * dt, 0, -MOVEMENT_SPEED * cos(gameState->yaw) * dt, 0),
		   up = VectorSet(0, 1, 0, 0),
		   right = VectorCross(forward, up);
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	VECTOR position = gameState->position;
	if (keys[SDL_SCANCODE_W]) position = VectorAdd(position, forward);
	if (keys[SDL_SCANCODE_A]) position = VectorSubtract(position, right);
	if (keys[SDL_SCANCODE_S]) position = VectorSubtract(position, forward);
	if (keys[SDL_SCANCODE_D]) position = VectorAdd(position, right);
	if (keys[SDL_SCANCODE_SPACE]) position = VectorAdd(position, VectorSet(0, MOVEMENT_SPEED * dt, 0, 0));
	if (keys[SDL_SCANCODE_LSHIFT]) position = VectorSubtract(position, VectorSet(0, MOVEMENT_SPEED * dt, 0, 0));
	gameState->position = position;
}


static void gameStateDraw(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	struct SpriteBatch *batch = gameState->batch;

	rendererDraw(&gameState->renderer, gameState->position, gameState->yaw, gameState->pitch, dt);

	// Draw GUI
	spriteBatchBegin(batch);
	// widgetValidate(gameState->flexLayout, 800, 600);
	// widgetDraw(gameState->flexLayout, batch);
	spriteBatchEnd(batch);
}

static void gameStateResize(struct State *state, int width, int height) {
	struct GameState *gameState = (struct GameState *) state;
	widgetLayout(gameState->flexLayout, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);

	// gameState->projection = MatrixPerspective(90.f, (float) width / height, zNear, zFar);
	// glUseProgram(gameState->program);
	// ALIGN(16) float mv[16];
	// glUniformMatrix4fv(gameState->projectionUniform, 1, GL_FALSE, MatrixGet(mv, gameState->projection));
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
	struct Renderer *renderer = &gameState->renderer;
	rendererInit(renderer);

	gameState->position = VectorSet(0, 0, 0, 1);
	gameState->yaw = 0;
	gameState->pitch = 0;
	gameState->objModel = loadModelFromObj("cube.obj");
	if (!gameState->objModel) {
		printf("Failed to load model.\n");
	}
	gameState->groundModel = loadModelFromObj("ground.obj");
	if (!gameState->groundModel) {
		printf("Failed to load ground model.\n");
	}
	renderer->instances[0].model = gameState->objModel;
	renderer->instances[1].model = gameState->groundModel;

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
