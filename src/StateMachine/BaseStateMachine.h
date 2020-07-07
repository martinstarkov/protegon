#pragma once

#include "Types.h"

class BaseState;

class BaseStateMachine {
public:
	virtual void update() = 0;
	virtual StateMachineID getStateMachineID() = 0;
	virtual BaseState* getCurrentState() = 0;
	virtual StateID getCurrentStateID() = 0;
	virtual bool isState(StateID state) = 0;
	virtual bool stateChangeOccured() = 0;
	virtual void initState(std::unique_ptr<BaseState> state) = 0;
	virtual void setCurrentState(std::unique_ptr<BaseState> state) = 0;
};