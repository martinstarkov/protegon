#pragma once

#include "System.h"

class PhysicsSystem : public System<TransformComponent, RigidBodyComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			auto [transform, rigidBodyC] = getComponents(id);
			Vec2D& position = transform.position;
			RigidBody& rigidBody = rigidBodyC.rigidBody;
			Vec2D& velocity = rigidBody.velocity;
			Vec2D& terminalVelocity = rigidBody.terminalVelocity;
			Vec2D& acceleration = rigidBody.acceleration;
			// gravity
			acceleration += rigidBody.gravity;
			// motion
			velocity += acceleration;
			// drag
			velocity *= (Vec2D(1.0) - rigidBody.drag);
			// terminal motion
			if (abs(velocity.x) > terminalVelocity.x) {
				velocity.x = Util::sgn(velocity.x) * terminalVelocity.x;
			} else if (abs(velocity.x) < LOWEST_VELOCITY) {
				velocity.x = 0.0;
			}
			if (abs(velocity.y) > terminalVelocity.y) {
				velocity.y = Util::sgn(velocity.y) * terminalVelocity.y;
			} else if (abs(velocity.y) < LOWEST_VELOCITY) {
				velocity.y = 0.0;
			}
			/*
			if (manager->hasComponent<PlayerController>(id)) {
				LOG("vel: " << rigidBody.velocity << ", tvel: " << rigidBody.terminalVelocity << ", drag: " << rigidBody.drag << ", accel: " << rigidBody.acceleration << ", gravity: " << rigidBody.gravity);
			}
			*/
		}
	}
};