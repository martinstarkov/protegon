#pragma once

#include "Types.h"
#include "../ECS/Entity.h"

class BaseState;

class BaseStateMachine {
public:
	virtual BaseStateMachine* clone() const = 0;
	virtual std::unique_ptr<BaseStateMachine> uniqueClone() const = 0;
	virtual void init(Entity handle) = 0;
	virtual void update() = 0;
	virtual StateMachineName getName() = 0;
	virtual void setName(StateMachineName name) = 0;
	virtual BaseState* getCurrentState() = 0;
	virtual void setCurrentState(StateName state) = 0;
	virtual bool inState(StateName name) = 0;
};