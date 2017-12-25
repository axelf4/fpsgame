#ifndef ENTITY_H
#define ENTITY_H

#include <string.h>
#include <vmath.h>
#include "model.h"

#define MAX_ENTITIES 128

typedef unsigned int Entity;

enum {
	POSITION_COMPONENT_MASK = 0x1,
	MODEL_COMPONENT_MASK = 0x2,
	VELOCITY_COMPONENT_MASK = 0x4,
	COLLIDER_COMPONENT_MASK = 0x8,
	ENEMY_COMPONENT_MASK = 0x10,
};

struct PositionComponent {
	VECTOR position;
};

struct ModelComponent {
	struct Model *model;
};

struct ColliderComponent {
	float radius;
};

struct EntityManager {
	unsigned int nextEntityIndex;
	unsigned int entityMasks[MAX_ENTITIES];
	struct PositionComponent positions[MAX_ENTITIES];
	struct ModelComponent models[MAX_ENTITIES];
	VECTOR velocities[MAX_ENTITIES];
	struct ColliderComponent colliders[MAX_ENTITIES];
};

void entityManagerInit(struct EntityManager *manager);

Entity entityManagerSpawn(struct EntityManager *manager);

void entityManagerClear(struct EntityManager *manager);

void entityManagerKill(struct EntityManager *manager, Entity entity);

#endif
