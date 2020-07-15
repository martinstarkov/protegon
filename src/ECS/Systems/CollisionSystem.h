#pragma once

#include "System.h"

// TODO: Redo entire collision system

class CollisionSystem : public System<TransformComponent, SizeComponent, CollisionComponent> {
public:
	virtual void update() override {
		for (auto& entityID : entities) {
			Entity& e = getEntity(entityID);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			SizeComponent* size = e.getComponent<SizeComponent>();
			MotionComponent* motion = e.getComponent<MotionComponent>();
			for (auto& otherID : entities) {
				if (entityID != otherID) { // FIX LATER: For some reason the red tree blocks don't collide with each other. Issue here?
					Entity& o = getEntity(otherID);
					TransformComponent* otherTransform = o.getComponent<TransformComponent>();
					SizeComponent* otherSize = o.getComponent<SizeComponent>();
					MotionComponent* otherMotion = o.getComponent<MotionComponent>();
					Vec2D relVel = Vec2D();
					if (motion && otherMotion) {
						relVel = otherMotion->velocity - motion->velocity;
					} else if (motion) {
						relVel = motion->velocity;
					} else if (otherMotion) {
						relVel = otherMotion->velocity;
					}
					Vec2D penetration = AABB(transform->position, size->size).colliding(AABB(AABB(otherTransform->position, otherSize->size)), relVel);
					if (penetration) {
						CollisionComponent* collider = e.getComponent<CollisionComponent>();
						CollisionComponent* otherCollider = o.getComponent<CollisionComponent>();
						collider->colliding = true;
						otherCollider->colliding = true;
						transform->position -= penetration;
						if (penetration.x) {
							//if (motion) {
							//	motion->velocity = Vec2D();
							//}
							//if (otherMotion) {
							//	otherMotion->velocity = Vec2D();
							//}
						}
						if (penetration.y) {
							//if (motion) {
							//	motion->velocity = Vec2D();
							//}
							//if (otherMotion) {
							//	otherMotion->velocity = Vec2D();
							//}
						}

					}
				}
			}
		}
	}
};