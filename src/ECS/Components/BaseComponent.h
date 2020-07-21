#pragma once

#include "../Types.h"

class Manager;

class BaseComponent {
public:
	virtual void init() = 0;
	virtual void setHandle(EntityID id, Manager* manager) = 0;
	virtual ComponentName getName() = 0;
	virtual ~BaseComponent() = default;
};