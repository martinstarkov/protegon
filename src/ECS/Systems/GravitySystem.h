#pragma once
#include "System.h"

class GravitySystem : public System<MotionComponent, GravityComponent> {
private:
	using BaseType = System<MotionComponent, GravityComponent>;
public:
	virtual void update() override {
		std::cout << "Gravity[" << _entities.size() << "],";
		for (auto& pair : _entities) {
			MotionComponent* motion = pair.second->getComponent<MotionComponent>();
			GravityComponent* gravity = pair.second->getComponent<GravityComponent>();
			motion->_velocity += gravity->_direction * gravity->_g;
		}
	}
};