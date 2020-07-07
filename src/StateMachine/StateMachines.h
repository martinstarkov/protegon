#pragma once

#include "StateMachine.h"

class WalkStateMachine : public StateMachine<WalkStateMachine> {
public:
	WalkStateMachine() {
		initState(std::make_unique<IdleState>());
	}
};
class JumpStateMachine : public StateMachine<JumpStateMachine> {
public:
	JumpStateMachine() {
		initState(std::make_unique<GroundedState>());
	}
};