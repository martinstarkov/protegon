#pragma once
#include "System.h"

class MovementSystem : public System<TransformComponent, MotionComponent> {
private:
	using BaseType = System<TransformComponent, MotionComponent>;
public:
	MovementSystem(Manager* manager) : BaseType(manager) {}
	virtual void update() override {
		std::cout << "Moving[" << _components.size() << "],";
		for (auto& componentTuple : _components) {
			TransformComponent* transform = std::get<TransformComponent*>(componentTuple);
			MotionComponent* motion = std::get<MotionComponent*>(componentTuple);
			motion->_velocity += motion->_acceleration;
			transform->_position += motion->_velocity;
		}
	}
};