#pragma once

#include "ecs/ECS.h"
#include <memory> // std::shared_ptr

namespace engine {

class BaseState;

class BaseStateMachine {
public:
	virtual void Init(ecs::Entity parent_entity) = 0;
	virtual void SetState(const char* name) = 0;
	// TEMPORARY
	virtual std::string GetState() = 0;
	virtual void Update() = 0;
protected:
	virtual void AddState(const char* name, std::shared_ptr<BaseState> state) = 0;
};

} // namespace engine