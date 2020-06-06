#pragma once

#include "../Types.h"
#include "../Entity.h"

class BaseSystem {
public:
	virtual void setManager(Manager* manager) = 0;
	virtual Entity& getEntity(EntityID entityID) = 0;
	virtual void onEntityChanged(EntityID entityID) = 0;
	virtual void onEntityDestroyed(EntityID entityID) = 0;
	virtual void update() = 0;
};