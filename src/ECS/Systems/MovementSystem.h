#pragma once

#include "System.h"

class MovementSystem : public System<TransformComponent, MotionComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			MotionComponent* motion = e.getComponent<MotionComponent>();
			transform->position += motion->velocity;
		}
	}
};