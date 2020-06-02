#pragma once
#include "System.h"

class MovementSystem : public System<TransformComponent, MotionComponent> {
private:
	using BaseType = System<TransformComponent, MotionComponent>;
public:
	virtual void update() override {
		std::cout << "Moving[" << _entities.size() << "],";
		for (auto& pair : _entities) {
			TransformComponent* transform = pair.second->getComponent<TransformComponent>();
			MotionComponent* motion = pair.second->getComponent<MotionComponent>();
			motion->_velocity += motion->_acceleration;
			transform->_position += motion->_velocity;
		}
	}
};