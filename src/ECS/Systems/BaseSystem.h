#pragma once

#include "../Types.h"

class Entity;

class BaseSystem {
public:
	virtual void setManager(Manager* manager) = 0;
	virtual void onEntityChanged(Entity* entity) = 0;
	virtual void onEntityDestroyed(EntityID entityID) = 0;
	virtual bool containsEntity(Entity* entity) = 0;
	virtual void update() = 0;
};