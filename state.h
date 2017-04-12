#ifndef STATE_H
#define STATE_H

typedef struct State {
	void (*update)(struct State *state, float dt);
	void (*draw)(struct State *state, float dt);
	void (*resize)(struct State *state, int width, int height);
} State;

typedef struct StateManager {
	struct State *state;
} StateManager;

void setState(struct StateManager *manager, struct State *state);

#endif
