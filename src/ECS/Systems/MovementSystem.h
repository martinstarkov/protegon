#pragma once

#include "System.h"

class MovementSystem : public System<TransformComponent, MotionComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			MotionComponent* motion = e.getComponent<MotionComponent>();
			transform->position += motion->velocity;
		}
	}
};