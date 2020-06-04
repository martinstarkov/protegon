#pragma once
#include "../Types.h"

class Entity;

class BaseSystem {
public:
	//virtual ~BaseSystem() = default;
	virtual void onEntityChanged(Entity* entity) = 0;
	virtual void onEntityDestroyed(EntityID entityID) = 0;
	virtual bool containsEntity(EntityID entityID) = 0;
	virtual void update() = 0;
};