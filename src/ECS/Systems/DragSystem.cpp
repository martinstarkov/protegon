#include "DragSystem.h"

#include "../Components/MotionComponent.h"
#include "../Components/DragComponent.h"

void DragSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		MotionComponent* motion = e.getComponent<MotionComponent>();
		DragComponent* drag = e.getComponent<DragComponent>();
		motion->velocity *= (Vec2D(1.0, 1.0) - drag->drag);
	}
}
