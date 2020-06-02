#pragma once
#include "./ECS/Types.h"
#include <limits>

template <typename T> inline ComponentID createComponentID() {
	return static_cast<ComponentID>(typeid(T).hash_code());
}

class Component {
public:
	virtual ~Component() = default;
	void setEntityID(EntityID entityID) { _entityID = entityID; }
protected:
	EntityID _entityID;
};