#pragma once

#include "BaseComponent.h"

#include "../Entity.h"

template <class T>
class Component : public BaseComponent {
public:
	// TODO: This constructor should be taking in the [parent entity] / [parent manager + entityID] reference
	// Each Component implementation will have a constructor that takes in the above mentioned information and passes it to the Component() constructor
	// Consider how this is effected by the construction of components in addComponents, they will each require a manager reference and an entity
	Component() = default;
	virtual void init() override {}
	virtual void setHandle(Entity handle) override final {
		entity = handle;
	}
	virtual ComponentName getName() override final {
		return typeid(T).name();
	}
protected:
	Entity entity;
};