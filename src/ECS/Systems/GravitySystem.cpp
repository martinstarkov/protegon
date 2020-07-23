#include "GravitySystem.h"

#include "SystemCommon.h"

void GravitySystem::update() {
	for (auto& id : entities) {
		Entity e = Entity(id, manager);
		MotionComponent* motion = e.getComponent<MotionComponent>();
		GravityComponent* gravity = e.getComponent<GravityComponent>();
		motion->velocity += gravity->direction * gravity->g;
	}
}
