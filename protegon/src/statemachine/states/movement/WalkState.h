#pragma once

#include "State.h"

#include "IdleState.h"
#include "RunState.h"

class WalkState : public engine::State {
public:
	virtual void OnEntry() override final {
		if (parent_entity.HasComponent<AnimationComponent>()) {
			auto& animation = parent_entity.GetComponent<AnimationComponent>();
			animation.current_animation = "walk";
			animation.counter = -1;
		}
	}
	virtual void Update() override final {
		assert(parent_entity.HasComponent<RigidBodyComponent>() && "Cannot update given state without RigidBodyComponent");
		auto& rigid_body = parent_entity.GetComponent<RigidBodyComponent>().rigid_body;
		if (abs(rigid_body.velocity.x) >= rigid_body.terminal_velocity.x * RUN_START_FRACTION) {
			parent_state_machine->SetState("run");
			return;
		} else if (abs(rigid_body.velocity.x) <= IDLE_START_VELOCITY) {
			parent_state_machine->SetState("idle");
			return;
		}
	}
};

