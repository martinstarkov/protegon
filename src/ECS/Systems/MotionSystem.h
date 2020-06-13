#pragma once

#include "System.h"

template <typename T> static int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}

class MotionSystem : public System<MotionComponent> {
public:
	virtual void update() override {
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			MotionComponent* motion = e.getComponent<MotionComponent>();
			motion->_velocity += motion->_acceleration;
			if (abs(motion->_velocity.x) > abs(motion->_terminalVelocity.x)) {
				motion->_velocity.x = sgn(motion->_velocity.x) * abs(motion->_terminalVelocity.x);
			}
			if (abs(motion->_velocity.y) > motion->_terminalVelocity.y) {
				motion->_velocity.y = sgn(motion->_velocity.y) * abs(motion->_terminalVelocity.y);
			}
		}
	}
};