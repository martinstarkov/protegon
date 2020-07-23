#include "StateMachines.h"

#include "States.h"

WalkStateMachine::WalkStateMachine(StateName initialState) : StateMachine(initialState) {
	states.emplace("idle", std::make_unique<IdleState>());
	states.emplace("walk", std::make_unique<WalkState>());
	states.emplace("run", std::make_unique<RunState>());
}

JumpStateMachine::JumpStateMachine(StateName initialState) : StateMachine(initialState) {
	states.emplace("grounded", std::make_unique<GroundedState>());
	states.emplace("jumped", std::make_unique<JumpState>());
}