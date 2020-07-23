#pragma once

#include "StateMachine.h"

class WalkStateMachine : public StateMachine {
public:
	WalkStateMachine(StateName initialState);
};

class JumpStateMachine : public StateMachine {
public:
	JumpStateMachine(StateName initialState);
};