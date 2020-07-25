#pragma once

#include "System.h"

class GravitySystem : public System<MotionComponent, GravityComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			MotionComponent* motion = e.getComponent<MotionComponent>();
			GravityComponent* gravity = e.getComponent<GravityComponent>();
			motion->velocity += gravity->direction * gravity->g;
		}
	}
};