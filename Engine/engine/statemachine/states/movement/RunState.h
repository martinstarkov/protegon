#pragma once

#include "State.h"

#define RUN_START_FRACTION 0.6 // run starts when velocity is this fraction of terminal velocity

#include "IdleState.h"

class RunState : public engine::State {
	virtual void OnEntry() override final {
		if (parent_entity.HasComponent<AnimationComponent>()) {
			auto& animation = parent_entity.GetComponent<AnimationComponent>();
			animation.current_animation = "run";
			animation.counter = -1;
		}
	}
	virtual void Update() override final {
		assert(parent_entity.HasComponent<RigidBodyComponent>() && "Cannot update given state without RigidBodyComponent");
		auto& rigid_body = parent_entity.GetComponent<RigidBodyComponent>().rigid_body;
		if (abs(rigid_body.velocity.x) < rigid_body.terminal_velocity.x * RUN_START_FRACTION) {
			parent_state_machine->SetState("walk");
			return;
		} else if (abs(rigid_body.velocity.x) <= IDLE_START_VELOCITY) {
			parent_state_machine->SetState("idle");
			return;
		}
	}
};