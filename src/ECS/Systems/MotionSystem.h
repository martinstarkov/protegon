#pragma once

#include "System.h"

class MotionSystem : public System<MotionComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			MotionComponent* motion = e.getComponent<MotionComponent>();
			motion->velocity += motion->acceleration;
			if (abs(motion->velocity.x) > abs(motion->terminalVelocity.x)) {
				motion->velocity.x = sgn(motion->velocity.x) * abs(motion->terminalVelocity.x);
			}
			if (abs(motion->velocity.y) > motion->terminalVelocity.y) {
				motion->velocity.y = sgn(motion->velocity.y) * abs(motion->terminalVelocity.y);
			}
		}
	}
};