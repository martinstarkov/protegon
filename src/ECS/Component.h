#pragma once
#include "Types.h"
#include <limits>

template <typename T> inline ComponentID createComponentID() {
	return static_cast<ComponentID>(typeid(T).hash_code());
}

class Component {
public:
	Component() : _entityID(INVALID_ENTITY_ID) {}
	virtual ~Component() = default;
	EntityID getEntityID() const { return _entityID; }
protected:
	EntityID _entityID;
};