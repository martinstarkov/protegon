#pragma once

#include "StateMachine.h"
#include "States.h"

class WalkStateMachine : public engine::StateMachine {
public:
	WalkStateMachine(ecs::Entity entity) {
		AddState("idle", std::make_shared<IdleState>());
		AddState("walk", std::make_shared<WalkState>());
		AddState("run", std::make_shared<RunState>());
		Init(entity);
	}
};

class JumpStateMachine : public engine::StateMachine {
public:
	JumpStateMachine(ecs::Entity entity) {
		AddState("grounded", std::make_shared<GroundedState>());
		AddState("jump", std::make_shared<JumpState>());
		Init(entity);
	}
};