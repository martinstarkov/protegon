#pragma once

#include "../Types.h"

class Entity;

class BaseComponent {
public:
	virtual void init() = 0;
	virtual ~BaseComponent() = default;
	virtual void setParentEntity(Entity* parentEntity) = 0;
	virtual ComponentID getComponentID() = 0;
	virtual ComponentName getComponentName() = 0;
};