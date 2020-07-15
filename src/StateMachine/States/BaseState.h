#pragma once

#include "../Types.h"

class BaseStateMachine;

class BaseState {
public:
	virtual void onEntry() = 0;
	virtual void onExit() = 0;
	virtual void update() = 0;
	virtual void setParentStateMachine(BaseStateMachine* parentStateMachine) = 0;
	virtual BaseStateMachine* getParentStateMachine() = 0;
	virtual StateID getStateID() = 0;
};

