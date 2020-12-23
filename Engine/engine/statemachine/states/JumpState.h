#pragma once

#include "State.h"

class JumpState : public engine::State {
public:
	virtual void Update() override final {
		assert(parent_entity.HasComponent<RigidBodyComponent>() && "Cannot update given state without RigidBodyComponent");
		auto& rigid_body = parent_entity.GetComponent<RigidBodyComponent>().rigid_body;
		// TODO: Change to check for collision instead of acceleration.
		if (rigid_body.acceleration.y >= 0.0) {
			parent_state_machine->SetState("grounded");
		}
	}
};