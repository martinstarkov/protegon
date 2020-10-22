#pragma once

#include "System.h"

#include <engine/renderer/AABB.h>

#include <algorithm> // std::sort
#include <vector> // std::vector

// TODO: Figure out relative velocities for two dynamic blocks.
// TODO: Figure out how to resolve out all static collision in one frame.

struct CollisionManifold {
	V2_double point;
	V2_double normal;
	double time = 0.0;
	friend std::ostream& operator<<(std::ostream& os, const CollisionManifold& obj) {
		os << "Point: " << obj.point << ", Normal: " << obj.normal << ", Time: " << obj.time;
		return os;
	}
};

struct Collision {
	ecs::Entity entity;
	CollisionManifold manifold;
};

// Sort times in order of lowest time first, if times are equal prioritize diagonal collisions first.
static void SortTimes(std::vector<Collision>& collisions) {
	std::sort(collisions.begin(), collisions.end(), [](const Collision& a, const Collision& b) {
		if (a.manifold.time != b.manifold.time) {
			return a.manifold.time < b.manifold.time;
		} else {
			return a.manifold.normal.MagnitudeSquared() < b.manifold.normal.MagnitudeSquared();
		}
	});
}

// Function which takes consecutive identical collision times such as [0.5: (0, -1), 0.5: (-1, 0)] and combines the normals into one time [0.5: (-1, -1)].
//static void CombineTimes(std::vector<Collision>& collisions) {
//	size_t size = collisions.size();
//	if (size > 1) {
//		std::vector<Collision> newCollisions;
//		for (size_t i = 0; i < size; ++i) {
//			static bool ignore = false;
//			if (!ignore) {
//				if (collisions[i].manifold.time == collisions[(i + 1) % size].manifold.time) {
//					if (collisions[i].manifold.normal.HasZero() && collisions[(i + 1) % size].manifold.normal.HasZero()) {
//						if (abs(collisions[i].manifold.normal.x) != abs(collisions[(i + 1) % size].manifold.normal.x)) {
//							Collision col = collisions[i];
//							col.manifold.normal += collisions[(i + 1) % size].manifold.normal;
//							newCollisions.push_back(col);
//							ignore = true;
//							continue;
//						}
//					}
//				}
//				newCollisions.push_back(collisions[i]);
//			} else {
//				ignore = false;
//			}
//		}
//		collisions = newCollisions;
//	}
//}

class CollisionSystem : public ecs::System<TransformComponent, CollisionComponent> {
public:
	virtual void Update() override final {
		// Color reset.
		for (auto [entity, transform, collision] : entities) {
			if (entity.HasComponent<RenderComponent>()) {
				entity.GetComponent<RenderComponent>().ResetColor();
			}
		}
		// Vector containing entities which have changed positions during the current cycle.
		std::vector<std::tuple<ecs::Entity, TransformComponent&, RigidBodyComponent&, CollisionComponent&>> static_check;
		static_check.reserve(entities.size()); // Reserve maximum possible capacity of moving entities.
		// Swept collision detection and resolution routine.
		for (auto [entity, transform, collision_component] : entities) {
			auto& collider = collision_component.collider;
			// Round the position to the nearest whole number, this ensures collision detection is precise and prevents tunneling. Very important.
			collider.position.x = engine::math::RoundCast<double>(transform.position.x);
			collider.position.y = engine::math::RoundCast<double>(transform.position.y);
			
			// Do not check static entities against other entities but the other way around.
			if (entity.HasComponent<RigidBodyComponent>()) {
				auto& rigid_body = entity.GetComponent<RigidBodyComponent>(); // Passed later to static check.
				auto& rb = rigid_body.rigid_body; // Makes code more readable as RigidBodyComponent is just a wrapper around RigidBody.
				// Vector of entities which the entity could potentially have collided with during its path.
				std::vector<ecs::Entity> broadphase_entities;
				broadphase_entities.reserve(entities.size()); // Reserve maximum possible capacity of collisions.
				auto broadphase = GetBroadphaseBox(rb.velocity, collider);
				for (auto [entity2, transform2, collision2] : entities) {
					if (entity != entity2) { // Do not check against self.
						if (AABBVsAABB(broadphase, collision2.collider)) {
							broadphase_entities.emplace_back(entity2);
						}
					}
				}
				// Vector of entities which collided with the swept path of the collider
				std::vector<Collision> collisions;
				collisions.reserve(entities.size());
				// Temporary object used inside each loop for passing by reference.
				Collision info;
				// Second sweep (narrow phase) collision detection.
				for (auto& entity2 : broadphase_entities) {
					auto& target_collider = entity2.GetComponent<CollisionComponent>().collider;
					if (DynamicAABBVsAABB(rb.velocity, collider, target_collider, info.manifold)) {
						info.entity = entity2;
						collisions.emplace_back(info);
					}
				}

				SortTimes(collisions);

				// A swept collision occurred.
				if (collisions.size() > 0) {

					// Store old velocity to see if it changes and complete second sweep.
					auto old_velocity = rb.velocity;

					// First sweep (narrow phase) collision resolution.
					for (auto& collision : collisions) {
						auto& target_collider = collision.entity.GetComponent<CollisionComponent>().collider;
						if (ResolveDynamicAABBVsAABB(rb.velocity, collider, target_collider, collision.manifold)) {
							// ... objects which were used to resolve the collision, not necessarily all touching objects.
						}
						// TODO: Limit collision coloring to only objects which touch the player.
						collision.entity.GetComponent<RenderComponent>().color = engine::RED;
					}

					// Velocity changed, complete a second sweep to ensure both axes have been swept.
					if (rb.velocity.x != old_velocity.x || rb.velocity.y != old_velocity.y) {
						// Clear first sweep collisions.
						collisions.clear();
						// Second sweep (narrow phase) collision detection.
						for (auto& entity2 : broadphase_entities) {
							auto& target_collider = entity2.GetComponent<CollisionComponent>().collider;
							if (DynamicAABBVsAABB(rb.velocity, collider, target_collider, info.manifold)) {
								info.entity = entity2;
								collisions.emplace_back(info);
							}
						}
						if (collisions.size() > 0) {

							SortTimes(collisions);
							// Second sweep (narrow phase) collision resolution.
							for (auto& collision : collisions) {
								auto& target_collider = collision.entity.GetComponent<CollisionComponent>().collider;
								ResolveDynamicAABBVsAABB(rb.velocity, collider, target_collider, collision.manifold);
							}
						}
					}
				}
				// Update collider position with resolved velocity.
				collider.position += rb.velocity;
				// If position has changed, update it and add entity to static collision check list.
				if (transform.position != collider.position) {
					transform.position = collider.position;
					static_check.emplace_back(entity, transform, rigid_body, collision_component);
				}
			}
		}
		// Static collision detection for objects which have moved due to sweeps (dynamic AABBs).
		// Important note: The static check mostly prevents objects from preferring to stay inside each other if both are dynamic (altough this still occurs in circumstances where the resolution would result in another static collision).
		for (auto [entity, transform, rigid_body, collision_component] : static_check) {
			auto& collider = collision_component.collider;
			// Check if there is currently an overlap with any other collider.
			for (auto [entity2, transform2, collision2] : entities) {
				if (entity != entity2) { // Do not check against self.
					auto& oCollider = collision2.collider;
					if (AABBVsAABB(collider, oCollider)) {
						auto collision_depth = IntersectAABB(collider, oCollider);
						if (!collision_depth.IsZero()) { // Static collision occured.
							// Resolve static collision.
							collider.position -= collision_depth;
						}
					}
				}
			}
			// Keep transform position updated.
			transform.position = collider.position;
		}
	}
	// Returns an AABB which encompasses the initial position and the future position of a dynamic AABB.
	AABB GetBroadphaseBox(const V2_double& velocity, const AABB& b) {
		AABB broadphase_box;
		broadphase_box.position.x = velocity.x > 0.0 ? b.position.x : b.position.x + velocity.x;
		broadphase_box.position.y = velocity.y > 0.0 ? b.position.y : b.position.y + velocity.y;
		broadphase_box.size.x = velocity.x > 0.0 ? velocity.x + b.size.x : b.size.x - velocity.x;
		broadphase_box.size.y = velocity.y > 0.0 ? velocity.y + b.size.y : b.size.y - velocity.y;
		return broadphase_box;
	}
	// Find the penetration of one AABB into another AABB
	V2_double IntersectAABB(const AABB& other_box, const AABB& box) {
		V2_double penetration;
		double dx = box.position.x - other_box.position.x;
		auto a_center = box.size / 2.0;
		auto b_center = other_box.size / 2.0;
		double px = (a_center.x + b_center.x) - std::abs(dx);
		if (px <= 0.0) {
			return penetration;
		}
		double dy = box.position.y - other_box.position.y;
		double py = (a_center.y + b_center.y) - std::abs(dy);
		if (py <= 0.0) {
			return penetration;
		}
		if (px < py) {
			double sx = engine::math::sgn(dx);
			penetration.x = px * sx;
		} else {
			double sy = engine::math::sgn(dy);
			penetration.y = py * sy;
		}
		return penetration;
	}
	// Check if two AABBs overlap
	bool AABBVsAABB(const AABB& a, const AABB& b) {
		// If any side of the aabb it outside the other aabb, there cannot be an overlap.
		if (a.position.x + a.size.x <= b.position.x || a.position.x >= b.position.x + b.size.x) return false;
		if (a.position.y + a.size.y <= b.position.y || a.position.y >= b.position.y + b.size.y) return false;
		return true;
	}
	// Check if a ray collides with an AABB.
	bool RayVsAABB(const V2_double& ray_origin, const V2_double& ray_dir, const AABB& target, CollisionManifold& out_collision) {

		// Initial condition: no collision normal.
		out_collision.normal = { 0.0, 0.0 };
		out_collision.point = { 0.0, 0.0 };

		// Cache division.
		auto inv_dir = 1.0 / ray_dir;

		// Calculate intersections with rectangle bounding axes.
		auto t_near = (target.position - ray_origin) * inv_dir;
		auto t_far = (target.position + target.size - ray_origin) * inv_dir;

		// Discard 0 / 0 divisions.
		if (std::isnan(t_far.y) || std::isnan(t_far.x)) return false;
		if (std::isnan(t_near.y) || std::isnan(t_near.x)) return false;

		// Sort axis collision times so t_near contains the shorter time.
		if (t_near.x > t_far.x) std::swap(t_near.x, t_far.x);
		if (t_near.y > t_far.y) std::swap(t_near.y, t_far.y);

		// Early rejection.
		if (t_near.x > t_far.y || t_near.y > t_far.x) return false;
		
		// Closest time will be the first contact.
		out_collision.time = std::max(t_near.x, t_near.y);

		// Furthest time is contact on opposite side of target.
		double t_hit_far = std::min(t_far.x, t_far.y);

		// Reject if furthest time is negative, meaning the object is travelling away from the target.
		if (t_hit_far < 0.0) {
			return false;
		}

		// Contact point of collision from parametric line equation.
		out_collision.point = ray_origin + out_collision.time * ray_dir;

		// Find which axis collides further along the movement time.
		if (t_near.x > t_near.y) { // X-axis.
			// Direction of movement.
			if (inv_dir.x < 0.0) {
				out_collision.normal = { 1.0, 0.0 };
			} else {
				out_collision.normal = { -1.0, 0.0 };
			}
		} else if (t_near.x < t_near.y) { // Y-axis.
			// Direction of movement.
			if (inv_dir.y < 0.0) {
				out_collision.normal = { 0.0, 1.0 };
			} else {
				out_collision.normal = { 0.0, -1.0 };
			}
		} else if (t_near.x == t_near.y && t_far.x == t_far.y) { // Both axes collide at the same time.
			// Diagonal collision, set normal to opposite of direction of movement.
			out_collision.normal = ray_dir.Identity().Opposite();
		}

		// Raycast collision occurred.
		return true;
	}
	// Determine the time at which a dynamic AABB would collide with a static AABB.
	bool DynamicAABBVsAABB(const V2_double& velocity, const AABB& dynamic_object, const AABB& static_target, CollisionManifold& collision) {

		// Check if dynamic object has a non-zero velocity. It cannot collide if it is not moving.
		if (velocity.IsZero()) {
			return false;
		}

		// Expand static target by dynamic object dimensions so that only the center of the dynamic object needs to be considered.
		AABB expanded_target{ static_target.position - dynamic_object.size / 2.0, static_target.size + dynamic_object.size };

		// Check if the velocity ray collides with the expanded target.
		if (RayVsAABB(dynamic_object.Center(), velocity, expanded_target, collision)) {
			return collision.time >= 0.0 && collision.time < 1.0;
		} else {
			return false;
		}
	}
	// Modify the velocity of a dynamic AABB so it does not collide with a static AABB.
	bool ResolveDynamicAABBVsAABB(V2_double& velocity, const AABB& dynamic_object, const AABB& static_target, const CollisionManifold& collision) {
		CollisionManifold repeat_check;
		// Repeat check is needed due to the fact that if multiple collisions are found, resolving the velocity for the nearest one may invalidate the previously thought collisions.
		if (DynamicAABBVsAABB(velocity, dynamic_object, static_target, repeat_check)) {
			velocity += collision.normal * abs(velocity) * (1.0 - collision.time);
			/*if (std::abs(velocity.x) < LOWEST_VELOCITY) {
				velocity.x = 0.0;
			}
			if (std::abs(velocity.y) < LOWEST_VELOCITY) {
				velocity.y = 0.0;
			}*/
			return true;
		}
		return false;
	}
	// Check for collision between two circles.
	bool CircleVsCircle(const Circle& a, const Circle& b) {
		double r = a.radius + b.radius;
		r *= r;
		return r < (a.position + b.position).MagnitudeSquared();
	}
	/*
	bool CirclevsCircle(CollisionManifold<Circle, Circle>& m) {
		// Setup a couple pointers to each object
		Circle& A = m.sA;
		Circle& B = m.sB;

		// Vector from A to B
		V2_double n = B.position - A.position;

		double r = A.radius + B.radius;
		r *= r;

		if (n.magnitudeSquared() > r) {
			return false;
		}
		// Circles have collided, now compute manifold
		double d = n.magnitude(); // perform actual sqrt

		// If distance between circles is not zero
		if (d) {
			// Distance is difference between radius and distance
			m.penetration = r - d;
			// Utilize our d since we performed sqrt on it already within magnitude()
			// Points from A to B, and is a unit vector
			m.normal = n / d;
			return true;
		} else { // Circles are in the same position
			// Choose random (but consistent) values
			m.penetration = A.radius;
			m.normal = V2_double{ 1.0, 0.0 };
			return true;
		}
	}
	bool AABBvsCircle(CollisionManifold<AABB, Circle>& m) {
		// Setup a couple pointers to each object
		AABB& A = m.sA;
		Circle& B = m.sB;
		// Vector from A to B
		V2_double n = B.position - A.position;
		// Closest point on A to center of B
		V2_double closest = n;
		// Clamp point to edges of the AABB
		closest.x = Util::clamp(-A.size.x / 2.0, A.size.x / 2.0, closest.x);
		closest.y = Util::clamp(-A.size.y / 2.0, A.size.y / 2.0, closest.y);
		bool inside = false;
		// Circle is inside the AABB, so we need to clamp the circle's center
		// to the closest edge
		if (n == closest) {
			inside = true;
			// Find closest axis
			if (abs(n.x) > abs(n.y)) {
				// Clamp to closest extent
				if (closest.x > 0.0) {
					closest.x = A.size.x / 2.0;
				} else {
					closest.x = -A.size.x / 2.0;
				}
			} else { // y axis is shorter
				// Clamp to closest extent
				if (closest.y > 0.0) {
					closest.y = A.size.y / 2.0;
				} else {
					closest.y = -A.size.y / 2.0;
				}
			}
		}

		V2_double normal = n - closest;
		double d = normal.magnitudeSquared();
		double r = B.radius;
		// Early out of the radius is shorter than distance to closest point and
		// Circle not inside the AABB
		if (d > r * r && !inside) {
			return false;
		}
		// Avoided sqrt until we needed
		d = sqrt(d);
		// Collision normal needs to be flipped to point outside if circle was
		// inside the AABB
		if (inside) {
			m.normal = -n;
			m.penetration = r - d;
		} else {
			m.normal = n;
			m.penetration = r - d;
		}

		return true;
	}
	*/
};