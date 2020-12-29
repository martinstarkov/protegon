#pragma once

#include "System.h"

class PhysicsSystem : public ecs::System<TransformComponent, RigidBodyComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, transform, rigid_body] : entities) {
			auto& rb = rigid_body.rigid_body;
			// Gravity.
			rb.acceleration += rb.gravity;
			// Motion.
			rb.velocity += rb.acceleration;
			// Drag.
			rb.velocity *= V2_double{ 1.0, 1.0 } - rb.drag;

			// Terminal motion.
			if (abs(rb.velocity.x) > rb.terminal_velocity.x) {
				rb.velocity.x = engine::math::Sign(rb.velocity.x) * rb.terminal_velocity.x;
			} else if (abs(rb.velocity.x) < LOWEST_VELOCITY) {
				rb.velocity.x = 0.0;
			}
			if (abs(rb.velocity.y) > rb.terminal_velocity.y) {
				rb.velocity.y = engine::math::Sign(rb.velocity.y) * rb.terminal_velocity.y;
			} else if (abs(rb.velocity.y) < LOWEST_VELOCITY) {
				rb.velocity.y = 0.0;
			}
		}
	}
};