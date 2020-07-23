#include "MotionSystem.h"

#include "SystemCommon.h"

#define MOTION_STOP 0.1

void MotionSystem::update() {
	for (auto& id : entities) {
		Entity e = Entity(id, manager);
		MotionComponent* motion = e.getComponent<MotionComponent>();
		motion->velocity += motion->acceleration;
		if (abs(motion->velocity.x) > motion->terminalVelocity.x) {
			motion->velocity.x = Util::sgn(motion->velocity.x) * motion->terminalVelocity.x;
		} else if (abs(motion->velocity.x) < LOWEST_VELOCITY) {
			motion->velocity.x = 0.0;
		}
		if (abs(motion->velocity.y) > motion->terminalVelocity.y) {
			motion->velocity.y = Util::sgn(motion->velocity.y) * motion->terminalVelocity.y;
		} else if (abs(motion->velocity.y) < LOWEST_VELOCITY) {
			motion->velocity.y = 0.0;
		}
	}
}
