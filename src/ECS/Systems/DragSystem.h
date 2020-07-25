#pragma once

#include "System.h"

class DragSystem : public System<DragComponent, MotionComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			MotionComponent* motion = e.getComponent<MotionComponent>();
			DragComponent* drag = e.getComponent<DragComponent>();
			motion->velocity *= (Vec2D(1.0, 1.0) - drag->drag);
		}
	}
};