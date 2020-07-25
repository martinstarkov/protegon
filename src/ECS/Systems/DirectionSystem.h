#pragma once

#include "System.h"

class DirectionSystem : public System<DirectionComponent, MotionComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			MotionComponent* motion = e.getComponent<MotionComponent>();
			DirectionComponent* direction = e.getComponent<DirectionComponent>();
			direction->previousDirection = direction->direction;
			if (motion->velocity.y == 0.0 && motion->velocity.x == 0.0) {
				direction->direction = Direction::DOWN;
			}
			if (motion->velocity.y < 0.0) {
				direction->direction = Direction::UP;
			} else if (motion->velocity.y > 0.0) {
				direction->direction = Direction::DOWN;
			}
			if (motion->velocity.x < 0.0) {
				direction->direction = Direction::LEFT;
			} else if (motion->velocity.x > 0.0) {
				direction->direction = Direction::RIGHT;
			}
		}
	}
};