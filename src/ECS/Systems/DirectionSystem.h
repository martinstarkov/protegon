#pragma once

#include "System.h"

#define IDLE_DIRECTION Direction::DOWN

class DirectionSystem : public System<DirectionComponent, RigidBodyComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			DirectionComponent& dc = *e.getComponent<DirectionComponent>();
			RigidBody& rigidBody = e.getComponent<RigidBodyComponent>()->rigidBody;
			Direction& direction = dc.direction;
			dc.previousDirection = direction;
			if (rigidBody.velocity.isZero()) {
				direction = IDLE_DIRECTION;
			}
			if (rigidBody.velocity.y < 0.0) {
				direction = Direction::UP;
			} else if (rigidBody.velocity.y > 0.0) {
				direction = Direction::DOWN;
			}
			if (rigidBody.velocity.x > 0.0) {
				direction = Direction::RIGHT;
			} else if (rigidBody.velocity.x < 0.0) {
				direction = Direction::LEFT;
			}
		}
	}
};