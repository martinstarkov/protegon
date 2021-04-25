#pragma once

#include <memory> // std::shared_ptr

#include "ecs/ECS.h"

namespace engine {

class BaseState;

class BaseStateMachine {
public:
	virtual void Init(const ecs::Entity& parent_entity, const char* starting_state) = 0;
	virtual void SetState(const char* name) = 0;
	// TEMPORARY
	virtual std::string GetState() = 0;
	virtual void Update() = 0;
protected:
	virtual void AddState(const char* name, std::shared_ptr<BaseState> state) = 0;
};

} // namespace engine