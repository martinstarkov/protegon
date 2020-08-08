#pragma once

#include "../Types.h"

class Manager;

class BaseSystem {
public:
	virtual void update() = 0;
	virtual void setManager(Manager* manager) = 0;
	virtual void onEntityChanged(EntityID id) = 0;
	virtual void onEntityCreated(EntityID id) = 0;
	virtual void onEntityDestroyed(EntityID id) = 0;
};