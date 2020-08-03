#pragma once

#include "System.h"

class PhysicsSystem : public System<TransformComponent, RigidBodyComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			Vec2D& position = e.getComponent<TransformComponent>()->position;
			RigidBody& rigidBody = e.getComponent<RigidBodyComponent>()->rigidBody;
			// gravity
			//rigidBody.acceleration += rigidBody.gravity;
			// motion
			//rigidBody.velocity += rigidBody.acceleration;
			// drag
			//rigidBody.velocity *= (Vec2D(1.0) - rigidBody.drag);
			// terminal motion
			/*if (abs(rigidBody.velocity.x) > rigidBody.terminalVelocity.x) {
				rigidBody.velocity.x = Util::sgn(rigidBody.velocity.x) * rigidBody.terminalVelocity.x;
			} else if (abs(rigidBody.velocity.x) < LOWEST_VELOCITY) {
				rigidBody.velocity.x = 0.0;
			}
			if (abs(rigidBody.velocity.y) > rigidBody.terminalVelocity.y) {
				rigidBody.velocity.y = Util::sgn(rigidBody.velocity.y) * rigidBody.terminalVelocity.y;
			} else if (abs(rigidBody.velocity.y) < LOWEST_VELOCITY) {
				rigidBody.velocity.y = 0.0;
			}*/
			if (e.hasComponent<PlayerController>()) {
				//LOG("vel: " << rigidBody.velocity << ", tvel: " << rigidBody.terminalVelocity << ", drag: " << rigidBody.drag << ", accel: " << rigidBody.acceleration << ", gravity: " << rigidBody.gravity);
			}
		}
	}
};