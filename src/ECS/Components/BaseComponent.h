#pragma once

#include "../Types.h"

class BaseComponent {
public:
	virtual ~BaseComponent() = default;
	virtual ComponentID getComponentID() = 0;
	virtual std::ostream& serialize(std::ostream& out) { return out; }
};