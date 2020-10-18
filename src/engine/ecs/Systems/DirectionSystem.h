#pragma once

#include "System.h"

#define IDLE_DIRECTION Direction::DOWN

class DirectionSystem : public ecs::System<DirectionComponent, RigidBodyComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, dir, rigid_body] : entities) {
			auto& direction = dir.direction;
			auto& rb = rigid_body.rigid_body;
			dir.previous_direction = direction;
			/*if (rb.velocity.IsZero()) {
				direction = IDLE_DIRECTION;
			}*/
			if (rb.velocity.y < 0.0) {
				direction = Direction::UP;
			} else if (rb.velocity.y > 0.0) {
				direction = Direction::DOWN;
			}
			if (rb.velocity.x > 0.0) {
				direction = Direction::RIGHT;
			} else if (rb.velocity.x < 0.0) {
				direction = Direction::LEFT;
			}
		}
	}
};