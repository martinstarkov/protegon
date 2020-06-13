#pragma once

#include "System.h"

class DragSystem : public System<DragComponent, MotionComponent> {
public:
	virtual void update() override {
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			MotionComponent* motion = e.getComponent<MotionComponent>();
			DragComponent* drag = e.getComponent<DragComponent>();
			motion->_velocity *= (Vec2D(1.0f, 1.0f) - drag->_drag);
		}
	}
};