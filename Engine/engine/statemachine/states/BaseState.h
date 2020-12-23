#pragma once

#include "ecs/ECS.h"

namespace engine {

class BaseStateMachine;

class BaseState {
public:
	virtual void OnEntry() = 0;
	virtual void OnExit() = 0;
	virtual void Update() = 0;
	virtual void SetParentEntity(ecs::Entity entity) = 0;
	virtual void SetParentStateMachine(BaseStateMachine* state_machine) = 0;
};

} // namespace engine