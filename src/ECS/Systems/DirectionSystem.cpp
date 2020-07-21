#include "DirectionSystem.h"

#include "../Components/DirectionComponent.h"
#include "../Components/MotionComponent.h"

void DirectionSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		MotionComponent* motion = e.getComponent<MotionComponent>();
		DirectionComponent* direction = e.getComponent<DirectionComponent>();
		if (motion->velocity.x < 0.0) {
			direction->xDirection = SDL_FLIP_HORIZONTAL;
		} else if (motion->velocity.x >= 0.0) {
			direction->xDirection = SDL_FLIP_NONE;
		}
		if (motion->velocity.y < 0.0) {
			direction->yDirection = SDL_FLIP_VERTICAL;
		} else if (motion->velocity.y >= 0.0) {
			direction->yDirection = SDL_FLIP_NONE;
		}
	}
}