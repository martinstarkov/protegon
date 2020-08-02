#pragma once

#include "../Types.h"

class Entity;

class BaseComponent {
public:
	virtual BaseComponent* clone() const = 0;
	virtual std::unique_ptr<BaseComponent> uniqueClone() const = 0;
	virtual void setup() = 0;
	virtual void serialize(nlohmann::json& j) = 0;
	virtual void setHandle(Entity handle) = 0;
	virtual ComponentName getName() = 0;
	virtual ~BaseComponent() = default;
};