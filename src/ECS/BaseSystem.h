#pragma once
#include "Types.h"

class Entity;
class Manager;

class BaseSystem {
protected:
	Manager* _manager;
public:
	explicit BaseSystem(Manager* manager) : _manager(manager) {}
	virtual ~BaseSystem() = default;

	virtual void onEntityCreated(Entity* entity) = 0;
	virtual void onEntityDestroyed(EntityID entityID) = 0;
	virtual void update() = 0;
};