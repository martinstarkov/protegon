#include "MovementSystem.h"

#include "../Components/TransformComponent.h"
#include "../Components/MotionComponent.h"

void MovementSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		TransformComponent* transform = e.getComponent<TransformComponent>();
		MotionComponent* motion = e.getComponent<MotionComponent>();
		transform->position += motion->velocity;
	}
}
