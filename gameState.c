#include "gameState.h"
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <SDL.h>
#include <GL/glew.h>
#include "pngloader.h"
#include "image.h"
#include "label.h"
#include "glUtil.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .02f
#define TURNING_TIME 1300.0f
#define DYING_TIME 3000.0f

static void onDie(struct GameState *gameState) {
	gameState->playerData.dead = 1;

	struct GuiContext *context = &gameState->context;
	guiSetRoot(context, (struct Widget *) &gameState->gameOverUI.label);
	widgetRequestLayout(context->root);
	widgetRequestFocus(context, context->root);
}

static void processEnemies(struct GameState *gameState, float dt) {
	struct EntityManager *manager = &gameState->manager;
	VECTOR playerPos = manager->positions[gameState->player].position;
	const unsigned mask = POSITION_COMPONENT_MASK | VELOCITY_COMPONENT_MASK | ENEMY_COMPONENT_MASK;
	for (int i = 0; i < MAX_ENTITIES; ++i) {
		if ((manager->entityMasks[i] & mask) == mask) {
			VECTOR pos = manager->positions[i].position;
			VECTOR toward = VectorSubtract(playerPos, pos);
			if ((VectorEqual(toward, VectorReplicate(0.0f)) & 0x7) == 0x7) {
				manager->velocities[i] = VectorReplicate(0.0f);
			} else {
				manager->velocities[i] = VectorDivide(Vector4Normalize(toward), VectorReplicate(100.0f));
			}
		}
	}
}

static void processVelocities(struct GameState *gameState, float dt) {
	struct EntityManager *manager = &gameState->manager;
	const unsigned mask = POSITION_COMPONENT_MASK | VELOCITY_COMPONENT_MASK;
	for (int i = 0; i < MAX_ENTITIES; ++i) {
		if ((manager->entityMasks[i] & mask) == mask) {
			manager->positions[i].position = VectorAdd(manager->positions[i].position,
					VectorMultiply(VectorReplicate(dt), manager->velocities[i]));
		}
	}
}

static void processCollisions(struct GameState *gameState, float dt) {
	struct EntityManager *manager = &gameState->manager;
	const unsigned mask = POSITION_COMPONENT_MASK | VELOCITY_COMPONENT_MASK | COLLIDER_COMPONENT_MASK;
	for (int i = 0; i < MAX_ENTITIES && i == gameState->player; ++i) {
		if ((manager->entityMasks[i] & mask) == mask) {
			VECTOR pos0 = manager->positions[i].position, velocity0 = manager->velocities[i];
			float radius0 = manager->colliders[i].radius;

			for (int j = i + 1; j < MAX_ENTITIES; ++j) {
				if ((manager->entityMasks[j] & mask) == mask) {
					VECTOR pos1 = manager->positions[j].position, velocity1 = manager->velocities[j];
					float radius1 = manager->colliders[j].radius;

					VECTOR movevec = VectorMultiply(VectorReplicate(dt), VectorSubtract(velocity0, velocity1));
					if (isSphereCollision(pos0, pos1, radius0, radius1, movevec)) {
						printf("hello collisions %f\n", dt);
						onDie(gameState);
					}
				}
			}
		}
	}
}

static float getTurningFactor(float turn) {
	float x = turn / TURNING_TIME;
	x = sin(0.5f * M_PI * x);
	return x;
}

static float calcDyingEffectFactor(float timer) {
	float t = timer >= DYING_TIME ? 1.0f : timer / DYING_TIME;
	return cubicBezier(0.0f, 0.07f, 0.59f, 1.0f, t);
}

static void gameStateUpdate(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	struct EntityManager *manager = &gameState->manager;
	const Uint8 *keys = SDL_GetKeyboardState(NULL);

	float timeScale = 1.0f;
	if (gameState->playerData.dead) {
		timeScale = 0.0f;
		gameState->playerData.deadTimer += dt;
	}
	dt *= timeScale;

	if (gameState->noclip) {
		int x, y;
		Uint32 button = SDL_GetRelativeMouseState(&x, &y);
		gameState->yaw -= x * MOUSE_SENSITIVITY;
		gameState->pitch -= y * MOUSE_SENSITIVITY;
	} else {
		// Handle turning
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
		gameState->yaw -= 0.006f * getTurningFactor(gameState->playerData.turn) * dt;
	}

	if (gameState->yaw > M_PI) gameState->yaw -= 2 * M_PI;
	else if (gameState->yaw < -M_PI) gameState->yaw += 2 * M_PI;
	// Clamp the pitch
	gameState->pitch = gameState->pitch < -M_PI / 2 ? -M_PI / 2 : gameState->pitch > M_PI / 2 ? M_PI / 2 : gameState->pitch;

	VECTOR forward = VectorSet(-MOVEMENT_SPEED * sin(gameState->yaw), 0, -MOVEMENT_SPEED * cos(gameState->yaw), 0),
		   up = VectorSet(0, 1, 0, 0),
		   right = VectorCross(forward, up);

	if (gameState->noclip) {
		VECTOR displacement = VectorReplicate(0.0f);
		if (keys[SDL_SCANCODE_W]) displacement = VectorAdd(displacement, forward);
		if (keys[SDL_SCANCODE_A]) displacement = VectorSubtract(displacement, right);
		if (keys[SDL_SCANCODE_S]) displacement = VectorSubtract(displacement, forward);
		if (keys[SDL_SCANCODE_D]) displacement = VectorAdd(displacement, right);
		if (keys[SDL_SCANCODE_SPACE]) displacement = VectorAdd(displacement, VectorSet(0, MOVEMENT_SPEED, 0, 0));
		if (keys[SDL_SCANCODE_LSHIFT]) displacement = VectorSubtract(displacement, VectorSet(0, MOVEMENT_SPEED, 0, 0));
		gameState->position = VectorAdd(gameState->position, VectorMultiply(VectorReplicate(dt), displacement));
	} else {
		manager->velocities[gameState->player] = forward;
	}

	processEnemies(gameState, dt);
	if (!gameState->playerData.dead) {
		processCollisions(gameState, dt);
	}
	processVelocities(gameState, dt);
}

static void gameStateDraw(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	struct SpriteBatch *batch = gameState->batch;
	struct EntityManager *manager = &gameState->manager;

	if (gameState->noclip) {
		rendererDraw(&gameState->renderer, gameState->position, gameState->yaw, gameState->pitch, 0.0f, dt);
	} else {
		VECTOR position = VectorAdd(manager->positions[gameState->player].position, VectorSet(0.0f, 1.4f, 0.0f, 0.0f));
		float yaw = gameState->yaw;
		float pitch = 0;
		float roll = M_PI / 9 * getTurningFactor(gameState->playerData.turn);
		if (gameState->playerData.dead) {
			float deadFactor = calcDyingEffectFactor(gameState->playerData.deadTimer);
			if (gameState->playerData.deadTimer > DYING_TIME / 2) yaw += 0.0002f * (gameState->playerData.deadTimer - DYING_TIME / 2);
			pitch = -M_PI / 7.0f * deadFactor;
			roll = (1.0f - deadFactor) * roll;
			float distFactor = 10.0f * deadFactor; // Distance from where we died
			position = VectorAdd(position, VectorSet(distFactor * cos(pitch) * sin(yaw), deadFactor * 6.0f, distFactor * cos(pitch) * cos(yaw), 0.0f));
			rendererSetEffectFactor(&gameState->renderer, deadFactor);
		} else {
			rendererSetEffectFactor(&gameState->renderer, 0.0f);
		}
		rendererDraw(&gameState->renderer, position, yaw, pitch, roll, dt);
	}

	// Draw GUI
	spriteBatchBegin(batch);
	struct GuiContext *context = &gameState->context;
	widgetValidate(context->root, 800, 600);
	widgetDraw(context->root, batch);
	spriteBatchEnd(batch);
}

static void gameStateResize(struct State *state, int width, int height) {
	struct GameState *gameState = (struct GameState *) state;
	rendererResize(&gameState->renderer, width, height);
	widgetLayout(gameState->context.root, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);
}

static void mouseDown(struct State *state, int button, int x, int y) {}
static void mouseUp(struct State *state, int button, int x, int y) {}

static void keyDown(struct State *state, SDL_Scancode scancode) {
	struct GameState *gameState = (struct GameState *) state;
	struct GuiContext *context = &gameState->context;
	guiKeyDown(context, scancode);
}

static void keyUp(struct State *state, SDL_Scancode scancode) {}

static struct FlexParams params0 = { ALIGN_END, -1, 100, UNDEFINED, 20, 0, 20, 20 },
						 params2 = {ALIGN_CENTER, 1, 100, UNDEFINED, 0, 0, 0, 50},
						 params1 = { ALIGN_CENTER, 1, UNDEFINED, UNDEFINED, 0, 0, 0, 0 };

static void initInGameUI(struct InGameUI *ui, struct Font *font) {
	labelInit((struct Widget *) &ui->scoreLabel, font, "Hello ben");
}

static void initGame(struct GameState *gameState) {
	struct EntityManager *manager = &gameState->manager;

	gameState->yaw = 0;
	gameState->pitch = 0;

	gameState->playerData.turn = 0.0f;
	gameState->playerData.dead = 0;
	gameState->playerData.deadTimer = 0.0f;

	entityManagerClear(manager);
	gameState->player = entityManagerSpawn(manager);
	manager->entityMasks[gameState->player] = POSITION_COMPONENT_MASK | VELOCITY_COMPONENT_MASK | COLLIDER_COMPONENT_MASK;
	manager->positions[gameState->player].position = VectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	manager->velocities[gameState->player] = VectorReplicate(0.0f);
	manager->colliders[gameState->player].radius = 0.2f;

	Entity ground = entityManagerSpawn(manager);
	manager->entityMasks[ground] = POSITION_COMPONENT_MASK | MODEL_COMPONENT_MASK;
	manager->positions[ground].position = VectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	manager->models[ground].model = gameState->groundModel;

	const float range = 400.0f;
	for (int i = 0; i < 35; ++i) {
		Entity enemy = entityManagerSpawn(manager);
		manager->entityMasks[enemy] = POSITION_COMPONENT_MASK | MODEL_COMPONENT_MASK | VELOCITY_COMPONENT_MASK | COLLIDER_COMPONENT_MASK | ENEMY_COMPONENT_MASK;
		manager->positions[enemy].position = VectorSet(range * randomFloat() - range / 2, 0.0f, range * randomFloat() - range / 2, 1.0f);
		manager->models[enemy].model = gameState->objModel;
		manager->velocities[enemy] = VectorReplicate(0.0f);
		manager->colliders[enemy].radius = 0.5f;
	}

	guiSetRoot(&gameState->context, (struct Widget *) &gameState->inGameUI.scoreLabel);
}

static void onGameOverKeyDown(struct Widget *widget, struct Event *event, void *data) {
	struct KeyEvent *keyEvent = (struct KeyEvent *) event;
	struct GameState *gameState = data;
	if (keyEvent->scancode == SDL_SCANCODE_SPACE) {
		printf("should start new game here\n");
		initGame(gameState);
	}
}

static void initGameOverUI(struct GameState *gameState, struct GameOverUI *ui, struct Font *font) {
	struct Widget *label = (struct Widget *) &ui->label;
	labelInit(label, font, "Game over.\nPress space to restart.");
	label->flags |= WIDGET_FOCUSABLE;
	widgetAddListener(label, (struct Listener) { "keyDown", onGameOverKeyDown, gameState, 0 });
}

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch, struct Font *font) {
	struct State *state = (struct State *) gameState;
	struct EntityManager *manager = &gameState->manager;
	state->update = gameStateUpdate;
	state->draw = gameStateDraw;
	state->resize = gameStateResize;
	state->mouseDown = mouseDown;
	state->mouseUp = mouseUp;
	state->keyDown = keyDown;
	state->keyUp = keyUp;
	gameState->batch = batch;
	gameState->font = font;
	entityManagerInit(manager);
	struct Renderer *renderer = &gameState->renderer;
	rendererInit(renderer, manager, 800, 600);

	gameState->position = VectorSet(0, 0, 0, 1);
	gameState->objModel = loadModelFromObj("assets/pyramid.obj");
	if (!gameState->objModel) {
		printf("Failed to load model.\n");
	}
	gameState->groundModel = loadModelFromObj("assets/ground.obj");
	if (!gameState->groundModel) {
		printf("Failed to load ground model.\n");
	}

	initInGameUI(&gameState->inGameUI, font);
	initGameOverUI(gameState, &gameState->gameOverUI, font);

	gameState->context = (struct GuiContext) { (struct Widget *) &gameState->inGameUI.scoreLabel };

	initGame(gameState);

	// Initialize GUI
	gameState->flexLayout = malloc(sizeof(struct FlexLayout));
	flexLayoutInitialize(gameState->flexLayout, DIRECTION_ROW, ALIGN_START);

	int width, height;
	gameState->cat = loadPngTexture("assets/cat.png", &width, &height);
	if (!gameState->cat) {
		fprintf(stderr, "Failed to load png image.\n");
	}
	gameState->image0 = malloc(sizeof(struct Image));
	imageInitialize(gameState->image0, gameState->cat, width, height, 0);
	gameState->image0->layoutParams = &params0;
	widgetAddChild(gameState->flexLayout, gameState->image0);

	gameState->image1 = malloc(sizeof(struct Image));
	imageInitialize(gameState->image1, gameState->cat, width, height, 0);
	gameState->image1->layoutParams = &params1;
	widgetAddChild(gameState->flexLayout, gameState->image1);

	gameState->label = malloc(sizeof(struct Label));
	labelInit(gameState->label, gameState->font, "Axel ffi! and the AV. HHHHHHHH Hi! (215): tv-hund. fesflhslg");
	gameState->label->layoutParams = &params2;
	widgetAddChild(gameState->flexLayout, gameState->label);

	gameState->noclip = 0;
}

void gameStateDestroy(struct GameState *gameState) {
	rendererDestroy(&gameState->renderer);
	destroyModel(gameState->objModel);

	widgetDestroy(gameState->flexLayout);
	free(gameState->flexLayout);
	free(gameState->image0);
	free(gameState->image1);
	labelDestroy(gameState->label);
	free(gameState->label);
	glDeleteTextures(1, &gameState->cat);
}
