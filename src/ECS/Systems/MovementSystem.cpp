#include "MovementSystem.h"

#include "SystemCommon.h"

void MovementSystem::update() {
	for (auto& id : entities) {
		Entity e = Entity(id, manager);
		TransformComponent* transform = e.getComponent<TransformComponent>();
		MotionComponent* motion = e.getComponent<MotionComponent>();
		transform->position += motion->velocity;
	}
}
