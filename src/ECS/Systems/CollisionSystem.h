#pragma once

#include "System.h"

class CollisionSystem : public System<TransformComponent, SizeComponent, CollisionComponent> {
public:
	virtual void update() override {
		//LOG_("Colliders[" << _entities.size() << "],");
		for (auto& entityID : _entities) {
			Entity& e = getEntity(entityID);
			TransformComponent* transform = e.getComponent<TransformComponent>();
			SizeComponent* size = e.getComponent<SizeComponent>();
			MotionComponent* motion = e.getComponent<MotionComponent>();
			for (auto& otherID : _entities) {
				if (entityID != otherID) {
					Entity& o = getEntity(otherID);
					TransformComponent* otherTransform = o.getComponent<TransformComponent>();
					SizeComponent* otherSize = o.getComponent<SizeComponent>();
					MotionComponent* otherMotion = o.getComponent<MotionComponent>();
					Vec2D relVel = Vec2D();
					if (motion && otherMotion) {
						relVel = otherMotion->_velocity - motion->_velocity;
					} else if (motion) {
						relVel = motion->_velocity;
					} else if (otherMotion) {
						relVel = otherMotion->_velocity;
					}
					Vec2D penetration = AABB(transform->_position, size->_size).colliding(AABB(AABB(otherTransform->_position, otherSize->_size)), relVel);
					if (penetration) {
						CollisionComponent* collider = e.getComponent<CollisionComponent>();
						CollisionComponent* otherCollider = o.getComponent<CollisionComponent>();
						collider->_colliding = true;
						otherCollider->_colliding = true;
						transform->_position -= penetration;
						if (penetration.x) {
							LOG("X COLLISION");
							if (motion) {
								motion->_velocity = Vec2D();
							}
							if (otherMotion) {
								otherMotion->_velocity = Vec2D();
							}
						}
						if (penetration.y) {
							LOG("Y COLLISION");
							if (motion) {
								motion->_velocity = Vec2D();
							}
							if (otherMotion) {
								otherMotion->_velocity = Vec2D();
							}
						}

					}
				}
			}
		}
	}
};