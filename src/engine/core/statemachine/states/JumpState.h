#pragma once

#include "State.h"

class JumpState : public State<JumpState> {
public:
	virtual void update() override final {
		if (entity.HasComponent<RigidBodyComponent>()) {
			auto& rb = entity.GetComponent<RigidBodyComponent>();
			auto& rigidBody = rb.rigidBody;
			// TODO: Change to check for collision instead of acceleration
			if (rigidBody.acceleration.y >= 0.0) {
				parentStateMachine->setCurrentState("grounded");
			}
		} else {
			assert(false && "Cannot update given state without RigidBodyComponent");
		}
	}
};