#pragma once

#include "StateMachine.h"
#include "States.h"

class WalkStateMachine : public engine::StateMachine {
public:
	WalkStateMachine(const ecs::Entity& entity) {
		AddState("idle", std::make_shared<IdleState>());
		AddState("walk", std::make_shared<WalkState>());
		AddState("run", std::make_shared<RunState>());
		Init(entity);
	}
};

class JumpStateMachine : public engine::StateMachine {
public:
	JumpStateMachine(const ecs::Entity& entity) {
		AddState("grounded", std::make_shared<GroundedState>());
		AddState("jump", std::make_shared<JumpState>());
		Init(entity);
	}
};

class ButtonStateMachine : public engine::StateMachine {
public:
	ButtonStateMachine(const ecs::Entity& entity) {
		AddState("default", std::make_shared<DefaultButtonState>());
		AddState("hover", std::make_shared<HoverButtonState>());
		AddState("focused", std::make_shared<FocusedButtonState>());
		AddState("active", std::make_shared<ActiveButtonState>());
		Init(entity);
	}
};