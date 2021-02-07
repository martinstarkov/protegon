#pragma once

#include "statemachine/State.h"

class GroundedState : public engine::State {
	virtual void Update() override final {
		assert(parent_entity.HasComponent<RigidBodyComponent>() && "Cannot update given state without RigidBodyComponent");
		auto& rigid_body = parent_entity.GetComponent<RigidBodyComponent>().rigid_body;
		if (rigid_body.acceleration.y < 0.0) { // Vertical movement upward.
			parent_state_machine->SetState("jump");
		}
	}
};