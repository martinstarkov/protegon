#pragma once

#include "BaseComponent.h"

template <class ComponentType>
class Component : public BaseComponent {
public:
	Component() {
		_id = static_cast<ComponentID>(typeid(ComponentType).hash_code());
	}
	virtual ComponentID getComponentID() override final { return _id; }
protected:
	ComponentID _id;
};