#pragma once

#include "System.h"

class GravitySystem : public System<MotionComponent, GravityComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			MotionComponent* motion = e.getComponent<MotionComponent>();
			GravityComponent* gravity = e.getComponent<GravityComponent>();
			motion->velocity += gravity->direction * gravity->g;
		}
	}
};