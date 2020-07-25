#pragma once

#include "StateMachine.h"

class WalkStateMachine : public StateMachine<WalkStateMachine> {
public:
	WalkStateMachine(StateName initialState);
};

class JumpStateMachine : public StateMachine<JumpStateMachine> {
public:
	JumpStateMachine(StateName initialState);
};