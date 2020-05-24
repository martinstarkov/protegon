#pragma once
#include "Components.h"

class PhysicsComponent : public Component {
public:
	PhysicsComponent() {
		_motionComponent = nullptr;
	}
	void init() override {
		if (!entity->has<MotionComponent>()) {
			entity->add<MotionComponent>();
		}
		if (!entity->has<TransformComponent>()) { // optional addition of default transform component
			entity->add<TransformComponent>();
		}
		_motionComponent = entity->get<MotionComponent>();
	}
	void update() override {
		_motionComponent->addVelocity(_motionComponent->getAcceleration());
		for (int i = 0; i < entity->count<TransformComponent>(); i++) {
			entity->get<TransformComponent>()->addPosition(_motionComponent->getVelocity());
		}
	}
private:
	MotionComponent* _motionComponent;
};