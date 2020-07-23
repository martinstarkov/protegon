#pragma once

#include "StateMachine.h"

class WalkStateMachine : public StateMachine {
public:
	WalkStateMachine(StateName initialState, Entity handle);
};

class JumpStateMachine : public StateMachine {
public:
	JumpStateMachine(StateName initialState, Entity handle);
};