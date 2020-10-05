#pragma once

#include "System.h"

class PhysicsSystem : public ecs::System<TransformComponent, RigidBodyComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, transform, rigid_body] : entities) {
			auto& rb = rigid_body.rigidBody;
			auto& terminal_velocity = rb.terminalVelocity;
			// gravity
			rb.acceleration += rb.gravity;
			// motion
			rb.velocity += rb.acceleration;
			// drag
			rb.velocity *= (Vec2D(1.0) - rb.drag);
			// terminal motion
			if (abs(rb.velocity.x) > terminal_velocity.x) {
				rb.velocity.x = engine::math::sgn(rb.velocity.x) * terminal_velocity.x;
			} else if (abs(rb.velocity.x) < LOWEST_VELOCITY) {
				rb.velocity.x = 0;
			}
			if (abs(rb.velocity.y) > terminal_velocity.y) {
				rb.velocity.y = engine::math::sgn(rb.velocity.y) * terminal_velocity.y;
			} else if (abs(rb.velocity.y) < LOWEST_VELOCITY) {
				rb.velocity.y = 0;
			}
			/*
			if (manager->hasComponent<PlayerController>(id)) {
				LOG("vel: " << rigidBody.velocity << ", tvel: " << rigidBody.terminalVelocity << ", drag: " << rigidBody.drag << ", accel: " << rigidBody.acceleration << ", gravity: " << rigidBody.gravity);
			}
			*/
		}
	}
};