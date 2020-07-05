#pragma once

#include "Types.h"

class BaseState;

class BaseStateMachine {
public:
	virtual void update() = 0;
	virtual StateMachineID getStateMachineID() = 0;
	virtual BaseState* getCurrentState() = 0;
	virtual void setCurrentState(BaseState* newState) = 0;
};