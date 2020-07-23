#include "DragSystem.h"

#include "SystemCommon.h"

void DragSystem::update() {
	for (auto& id : entities) {
		Entity e = Entity(id, manager);
		MotionComponent* motion = e.getComponent<MotionComponent>();
		DragComponent* drag = e.getComponent<DragComponent>();
		motion->velocity *= (Vec2D(1.0, 1.0) - drag->drag);
	}
}
