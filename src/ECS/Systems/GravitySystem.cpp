#include "GravitySystem.h"

#include "../Components/MotionComponent.h"
#include "../Components/GravityComponent.h"

void GravitySystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		MotionComponent* motion = e.getComponent<MotionComponent>();
		GravityComponent* gravity = e.getComponent<GravityComponent>();
		motion->velocity += gravity->direction * gravity->g;
	}
}
