#pragma once

#include "State.h"

#define IDLE_START_VELOCITY 0.5 // idle starts when velocity is less than or equal to this value

class IdleState : public engine::State {
	virtual void OnEntry() override final {
		if (parent_entity.HasComponent<AnimationComponent>()) {
			auto& animation = parent_entity.GetComponent<AnimationComponent>();
			animation.current_animation = "idle";
			animation.counter = -1;
		}
	}
	virtual void Update() override final {
		assert(parent_entity.HasComponent<RigidBodyComponent>() && "Cannot update given state without RigidBodyComponent");
		auto& rigid_body = parent_entity.GetComponent<RigidBodyComponent>().rigid_body;
		if (abs(rigid_body.velocity.x) >= IDLE_START_VELOCITY) {
			parent_state_machine->SetState("walk");
			return;
		}
	}
};