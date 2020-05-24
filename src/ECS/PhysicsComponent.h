#pragma once
#include "Components.h"

class PhysicsComponent : public Component {
public:
	PhysicsComponent() {
		_transformComponent = nullptr;
		_motionComponent = nullptr;
	}
	void init() override {
		_transformComponent = &entity->get<TransformComponent>(true);
		_motionComponent = &entity->get<MotionComponent>(true);
	}
	void update() override {
		_motionComponent->addVelocity(_motionComponent->getAcceleration());
		_transformComponent->addPosition(_motionComponent->getVelocity());
	}
private:
	TransformComponent* _transformComponent;
	MotionComponent* _motionComponent;
};