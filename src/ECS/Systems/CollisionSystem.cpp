#include "CollisionSystem.h"

#include "../Components/TransformComponent.h"
#include "../Components/SizeComponent.h"
#include "../Components/MotionComponent.h"
#include "../Components/CollisionComponent.h"
#include "../../AABB.h"

void CollisionSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		TransformComponent* transform = e.getComponent<TransformComponent>();
		SizeComponent* size = e.getComponent<SizeComponent>();
		MotionComponent* motion = e.getComponent<MotionComponent>();
		for (auto& oId : entities) {
			if (id != oId) { // FIX LATER: For some reason the red tree blocks don't collide with each other. Issue here?
				EntityHandle o = EntityHandle(oId, manager);
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
				}
			}
		}
	}
}
