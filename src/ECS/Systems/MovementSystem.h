#pragma once

#include "System.h"

class MovementSystem : public System<TransformComponent, MotionComponent> {
public:
	virtual void update() override {
		//LOG_("Moving[" << _entities.size() << "],");
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			MotionComponent* motion = e.getComponent<MotionComponent>();
			motion->_velocity += motion->_acceleration;
			transform->_position += motion->_velocity;
		}
	}
};