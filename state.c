#include "state.h"
#include <assert.h>

void setState(struct StateManager *manager, struct State *state) {
	assert(state && "State can not be null.");
	manager->state = state;
}
