#pragma once

#include "Types.h"
#include "../ECS/EntityHandle.h"

class BaseState;

class BaseStateMachine {
public:
	virtual void init(StateName initialState, EntityHandle handle) = 0;
	virtual void update() = 0;
	virtual StateMachineName getName() = 0;
	virtual void setName(StateMachineName name) = 0;
	virtual BaseState* getCurrentState() = 0;
	virtual void setCurrentState(StateName state) = 0;
	virtual bool inState(StateName name) = 0;
};