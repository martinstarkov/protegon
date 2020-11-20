#pragma once

#include "System.h"

class PhysicsSystem : public ecs::System<TransformComponent, RigidBodyComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, transform, rigid_body] : entities) {
			auto& rb = rigid_body.rigid_body;
			// Gravity.
			rb.acceleration += rb.gravity;

			//rb.acceleration.x += -engine::math::sgn(rb.velocity.x) / rb.mass * 0.5 * rb.drag.Magnitude() * std::pow(rb.velocity.Magnitude(), 2) * 1;
			//rb.acceleration.y += engine::math::sgn(rb.velocity.y) / rb.mass * 0.5 * rb.drag.Magnitude() * std::pow(rb.velocity.Magnitude(), 2) * 1;
			// Motion.
			rb.velocity += rb.acceleration;

			rb.velocity *= 0.1;
			// Drag.
			//rb.velocity *= V2_double{ 1.0, 1.0 } - rb.drag;

			// Terminal motion.
			/*if (abs(rb.velocity.x) > rb.terminal_velocity.x) {
				rb.velocity.x = engine::math::sgn(rb.velocity.x) * rb.terminal_velocity.x;
			} else if (abs(rb.velocity.x) < LOWEST_VELOCITY) {
				rb.velocity.x = 0.0;
			}
			if (abs(rb.velocity.y) > rb.terminal_velocity.y) {
				rb.velocity.y = engine::math::sgn(rb.velocity.y) * rb.terminal_velocity.y;
			} else if (abs(rb.velocity.y) < LOWEST_VELOCITY) {
				rb.velocity.y = 0.0;
			}*/
		}
	}
};