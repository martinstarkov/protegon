#pragma once

#include "Types.h"
#include "../ECS/Entity.h"

class BaseState;

class BaseStateMachine {
public:
	BaseStateMachine() = default;
	BaseStateMachine(const BaseStateMachine&) = delete;
	BaseStateMachine(BaseStateMachine&&) = delete;
	virtual void init(Entity handle) = 0;
	virtual void update() = 0;
	virtual StateMachineName getName() = 0;
	virtual void setName(StateMachineName name) = 0;
	virtual BaseState* getCurrentState() = 0;
	virtual void setCurrentState(StateName state) = 0;
	virtual bool inState(StateName name) = 0;
};