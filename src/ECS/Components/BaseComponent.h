#pragma once

#include "../Types.h"

class BaseComponent {
public:
	virtual ComponentID getComponentID() = 0;
	virtual void setEntityID(EntityID entityID) = 0;
};