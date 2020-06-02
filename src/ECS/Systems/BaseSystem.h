#pragma once
#include "../Types.h"

class Entity;

class BaseSystem {
public:
	virtual ~BaseSystem() = default;
	virtual void onEntityCreated(Entity* entity) = 0;
	virtual void onEntityDestroyed(EntityID entityID) = 0;
	virtual void update() = 0;
};