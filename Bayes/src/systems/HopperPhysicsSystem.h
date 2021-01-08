#pragma once

#include <engine/Include.h>

#include "components/Components.h"

class HopperPhysicsSystem : public ecs::System<TransformComponent, RigidBodyComponent, HopperComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, transform, rigid_body, hopper] : entities) {
			auto& rb = rigid_body.rigid_body;
			// Gravity.
			rb.acceleration += rb.gravity;
			// Motion.
			rb.velocity += rb.acceleration;
			// Slow motion down
			rb.velocity *= 0.1;
			hopper.theta_d += hopper.theta_dd;
			hopper.theta_d *= (1 - 0.1);
			LOG(hopper.theta_d);
			//LOG(hopper.theta_d);
			transform.rotation += hopper.theta_d;
			transform.rotation = (int)transform.rotation % 360;
		}
	}
};