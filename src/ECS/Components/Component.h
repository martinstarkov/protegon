#pragma once

#include "BaseComponent.h"

#include "../EntityHandle.h"

template <class ComponentType>
class Component : public BaseComponent {
public:
	// TODO: This constructor should be taking in the [parent entity] / [parent manager + entityID] reference
	// Each Component implementation will have a constructor that takes in the above mentioned information and passes it to the Component() constructor
	// Consider how this is effected by the construction of components in addComponents, they will each require a manager reference and an entity
	// Better ways of accomplishing the above? Pass all entity tied components a manager reference afterward somehow?
	Component() {}
	virtual void init() override {}
	virtual void setHandle(EntityID id, Manager* manager) override final {
		entity = EntityHandle(id, manager);
	}
	virtual ComponentName getName() override final { return typeid(ComponentType).name(); }
protected:
	EntityHandle entity;
};