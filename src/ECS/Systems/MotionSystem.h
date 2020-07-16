#pragma once

#include "System.h"

#define MOTION_STOP 0.1

class MotionSystem : public System<MotionComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			MotionComponent* motion = e.getComponent<MotionComponent>();
			motion->velocity += motion->acceleration;
			if (abs(motion->velocity.x) > motion->terminalVelocity.x) {
				motion->velocity.x = sgn(motion->velocity.x) * motion->terminalVelocity.x;
			} else if (abs(motion->velocity.x) < LOWEST_VELOCITY) {
				motion->velocity.x = 0.0;
			}
			if (abs(motion->velocity.y) > motion->terminalVelocity.y) {
				motion->velocity.y = sgn(motion->velocity.y) * motion->terminalVelocity.y;
			} else if (abs(motion->velocity.y) < LOWEST_VELOCITY) {
				motion->velocity.y = 0.0;
			}
		}
	}
};