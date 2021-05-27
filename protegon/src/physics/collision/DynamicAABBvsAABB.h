//#pragma once
//
//#include "renderer/AABB.h"
//
//#include "physics/Collision.h"
//#include "physics/collision/static/LinevsAABB.h"
//
//namespace engine {
//
//namespace collision {
//
//// Determine the time at which a dynamic AABB would collide with a static AABB.
//static bool DynamicAABBvsAABB(const V2_double& velocity, const AABB& dynamic_object, const AABB& static_target, CollisionManifold& collision) {
//
//	// Check if dynamic object has a non-zero velocity. It cannot collide if it is not moving.
//	if (velocity.IsZero()) {
//		return false;
//	}
//
//	// Expand static target by dynamic object dimensions so that only the center of the dynamic object needs to be considered.
//	AABB expanded_target{ static_target.position - dynamic_object.size / 2.0, static_target.size + dynamic_object.size };
//
//	// Check if the velocity ray collides with the expanded target.
//	if (LinevsAABB(dynamic_object.Center(), velocity, expanded_target, collision)) {
//		return collision.time >= 0.0 && collision.time < 1.0;
//	} else {
//		return false;
//	}
//}
//
//// Modify the velocity of a dynamic AABB so it does not collide with a static AABB.
//static bool ResolveDynamicAABBvsAABB(V2_double& velocity, const AABB& dynamic_object, const AABB& static_target, const CollisionManifold& collision) {
//	CollisionManifold repeat_check;
//	// Repeat check is needed due to the fact that if multiple collisions are found, resolving the velocity for the nearest one may invalidate the previously thought collisions.
//	if (DynamicAABBvsAABB(velocity, dynamic_object, static_target, repeat_check)) {
//		velocity += collision.normal * Abs(velocity) * (1.0 - collision.time);
//		return true;
//	}
//	return false;
//}
//
//} // namespace collision
//
//} // namespace engine


//	AABB GetBroadphaseBox(const V2_double& velocity) const {
//		AABB broadphase_box;
//		broadphase_box.position.x = velocity.x > 0.0 ? position.x : position.x + velocity.x;
//		broadphase_box.position.y = velocity.y > 0.0 ? position.y : position.y + velocity.y;
//		broadphase_box.size.x = velocity.x > 0.0 ? velocity.x + size.x : size.x - velocity.x;
//		broadphase_box.size.y = velocity.y > 0.0 ? velocity.y + size.y : size.y - velocity.y;
//		return broadphase_box;
//	}