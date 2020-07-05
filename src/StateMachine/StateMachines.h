#pragma once

#include "StateMachine.h"

class WalkStateMachine : public StateMachine<WalkStateMachine> {
public:
	WalkStateMachine() {
		setCurrentState(new IdleState());
	}
};
class JumpStateMachine : public StateMachine<JumpStateMachine> {
public:
	JumpStateMachine() {
		setCurrentState(new GroundedState());
	}
};