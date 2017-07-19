#include "entity.h"

void entityManagerInit(struct EntityManager *manager) {
	manager->nextEntityIndex = 0;
	memset(manager->entityMasks, 0, sizeof manager->entityMasks);
}

Entity entityManagerSpawn(struct EntityManager *manager) {
	// TODO reuse entity indices
	return manager->nextEntityIndex++;
}

void entityManagerKill(struct EntityManager *manager, Entity entity) {
	// TODO
}
