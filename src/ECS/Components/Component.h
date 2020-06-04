#pragma once
#include "BaseComponent.h"

//template <typename T> inline ComponentID createComponentID() {
//	return static_cast<ComponentID>(typeid(T).hash_code());
//}

#define UNKNOWN_ENTITY_ID -1

template <class ComponentType>
class Component : public BaseComponent {
public:
	Component() {
		ID = static_cast<ComponentID>(typeid(ComponentType).hash_code());
		_entityID = UNKNOWN_ENTITY_ID;
	}
	//virtual ~Component() = default;
	virtual ComponentID getComponentID() override final { return ID; }
	virtual void setEntityID(EntityID entityID) override final { _entityID = entityID; }
protected:
	ComponentID ID;
	EntityID _entityID;
};