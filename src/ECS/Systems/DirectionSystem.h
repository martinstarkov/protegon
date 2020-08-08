#pragma once

#include "System.h"

#define IDLE_DIRECTION Direction::DOWN

class DirectionSystem : public System<DirectionComponent, RigidBodyComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			auto [dir, rigidBodyC] = getComponents(id);
			Direction& direction = dir.direction;
			RigidBody& rigidBody = rigidBodyC.rigidBody;
			dir.previousDirection = direction;
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