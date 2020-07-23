#pragma once

#include "../Types.h"

class Entity;

class BaseComponent {
public:
	virtual void init() = 0;
	virtual void setHandle(Entity handle) = 0;
	virtual ComponentName getName() = 0;
	virtual ~BaseComponent() = default;
};