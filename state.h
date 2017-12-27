#ifndef STATE_H
#define STATE_H

#include <SDL.h>

typedef struct State {
	void (*update)(struct State *state, float dt);
	void (*draw)(struct State *state, float dt);
	void (*resize)(struct State *state, int width, int height);
	void (*mouseDown)(struct State *state, int button, int x, int y);
	void (*mouseUp)(struct State *state, int button, int x, int y);
	void (*keyDown)(struct State *state, SDL_Scancode scancode);
	void (*keyUp)(struct State *state, SDL_Scancode scancode);
} State;

typedef struct StateManager {
	struct State *state;
} StateManager;

void setState(struct StateManager *manager, struct State *state);

#endif
