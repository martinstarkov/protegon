#pragma once

#include "StateMachine.h"

class WalkStateMachine : public StateMachine<WalkStateMachine> {
public:
	WalkStateMachine() {
		states.emplace("idle", std::make_unique<IdleState>());
		states.emplace("walk", std::make_unique<WalkState>());
		states.emplace("run", std::make_unique<RunState>());
	}
};
class JumpStateMachine : public StateMachine<JumpStateMachine> {
public:
	JumpStateMachine() {
		states.emplace("grounded", std::make_unique<GroundedState>());
		states.emplace("jumped", std::make_unique<JumpState>());
	}
};