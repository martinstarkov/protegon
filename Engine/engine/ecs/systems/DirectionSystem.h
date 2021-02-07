#pragma once

#include "ecs/System.h"

#include "utils/Direction.h"

#define IDLE_DIRECTION Direction::DOWN

class DirectionSystem : public ecs::System<DirectionComponent, RigidBodyComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, dir, rigid_body] : entities) {
			auto& x_direction = dir.x_direction;
			auto& y_direction = dir.y_direction;
			auto& rb = rigid_body.rigid_body;
			dir.x_previous_direction = x_direction;
			dir.y_previous_direction = y_direction;
			/*if (rb.velocity.IsZero()) {
				direction = IDLE_DIRECTION;
			}*/
			// Flip Y-direction if moving up.
			/*if (rb.velocity.y < 0.0) {
				y_direction = Direction::UP;
			} else if (rb.velocity.y > 0.0) {
				y_direction = Direction::DOWN;
			}*/
			// Flip X-direction if moving left.
			if (rb.velocity.x > 0.0) {
				x_direction = Direction::RIGHT;
			} else if (rb.velocity.x < 0.0) {
				x_direction = Direction::LEFT;
			}
		}
	}
};