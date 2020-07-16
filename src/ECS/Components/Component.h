#pragma once

#include "BaseComponent.h"

template <class ComponentType>
class Component : public BaseComponent {
public:
	Component() {
		_id = static_cast<ComponentID>(typeid(ComponentType).hash_code());
	}
	virtual void init() override {}
	virtual void setParentEntity(Entity* newParentEntity) override final {
		parentEntity = newParentEntity;
	}
	virtual ComponentID getComponentID() override final { return _id; }
	virtual ComponentName getComponentName() override final { return typeid(ComponentType).name(); }
protected:
	Entity* parentEntity;
private:
	ComponentID _id;
};