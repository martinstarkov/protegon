#pragma once

#include "System.h"

class MovementSystem : public System<TransformComponent, MotionComponent> {
public:
	virtual void update() override {
		//std::cout << "Moving[" << _entities.size() << "],";
		for (auto& e : _entities) {
			TransformComponent* transform = e->getComponent<TransformComponent>();
			MotionComponent* motion = e->getComponent<MotionComponent>();
			motion->_velocity += motion->_acceleration;
			transform->_position += motion->_velocity;
		}
	}
};