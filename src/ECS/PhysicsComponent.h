#pragma once
#include "Components.h"

class PhysicsComponent : public Component {
public:
	PhysicsComponent() {
		_motionComponent = nullptr;
	}
	void init() override {
		if (entity->has<MotionComponent>()) {
			_motionComponent = entity->get<MotionComponent>();
		} else {
			_motionComponent = entity->add<MotionComponent>();
		}
		add(_motionComponent);
	}
	void update() override {
		_motionComponent->addVelocity(_motionComponent->getAcceleration());
		for (auto c : entity->getComponents<TransformComponent>()) {
			c->addPosition(_motionComponent->getVelocity());
		}
	}
private:
	MotionComponent* _motionComponent;
};