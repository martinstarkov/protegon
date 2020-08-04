#pragma once

#include "System.h"

#include "../../Game.h"

#include <algorithm>

// TODO: Redo entire collision system

struct CollisionManifold {
	Vec2D point;
	Vec2D normal;
	double time = 0.0;
};

struct Collision {
	Entity entity;
	CollisionManifold manifold;
};

#define DC SDL_Color{ 0, 0, 0, 255 }

class CollisionSystem : public System<TransformComponent, RigidBodyComponent, CollisionComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			manager->getComponent<CollisionComponent>(id)->collider.position = manager->getComponent<TransformComponent>(id)->position;
			RenderComponent* render = manager->getComponent<RenderComponent>(id);
			if (render) {
				render->color = { 0, 0, 0, 255 };
			}
		}
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			if (e.hasComponent<PlayerController>()) {
				TransformComponent& transform = *e.getComponent<TransformComponent>();
				RigidBody& rigidBody = e.getComponent<RigidBodyComponent>()->rigidBody;
				AABB& collider = e.getComponent<CollisionComponent>()->collider;
				std::vector<Collision> collisions;
				EntitySet broadphaseEntities;
				AABB broadphase = getSweptBroadphaseBox(rigidBody.velocity, collider);
				Game::aabbs.push_back({ broadphase, { 255, 0, 0, 255 } });
				for (auto& oId : entities) {
					if (oId == id) continue;
					if (AABBvsAABB(broadphase, manager->getComponent<CollisionComponent>(oId)->collider)) {
						broadphaseEntities.insert(oId);
					}
				}
				Collision info;
				for (auto& oId : broadphaseEntities) {
					AABB& otherCollider = manager->getComponent<CollisionComponent>(oId)->collider;
					if (DynamicRectVsRect(&rigidBody, &collider, otherCollider, info.manifold)) {
						info.entity = Entity(oId, manager);
						collisions.push_back(info);
					}
				}
				// Sort collision sweep times
				std::sort(collisions.begin(), collisions.end(), [](const Collision& a, const Collision& b) {
					return a.manifold.time < b.manifold.time;
				});
				if (collisions.size() > 0) {
					Vec2D oldVelocity = rigidBody.velocity;
					for (auto& collision : collisions) {
						AABB& otherCollider = collision.entity.getComponent<CollisionComponent>()->collider;
						RenderComponent* render = collision.entity.getComponent<RenderComponent>();
						if (ResolveDynamicRectVsRect(&rigidBody, &collider, &otherCollider)) {
							if (render) {
								render->color = { 255, 0, 255, 255 };
							}
						} else {
							if (render) {
								render->color = { 255, 0, 0, 255 };
							}
						}
					}
					if (rigidBody.velocity != oldVelocity) { // velocity changed, complete a second sweep
						collisions.clear();
						for (auto& oId : broadphaseEntities) {
							AABB& otherCollider = manager->getComponent<CollisionComponent>(oId)->collider;
							if (DynamicRectVsRect(&rigidBody, &collider, otherCollider, info.manifold)) {
								info.entity = Entity(oId, manager);
								collisions.push_back(info);
							}
						}
						if (collisions.size() > 0) {
							// Sort second collision sweep times
							std::sort(collisions.begin(), collisions.end(), [](const Collision& a, const Collision& b) {
								return a.manifold.time < b.manifold.time;
							});
							for (auto& collision : collisions) {
								AABB& otherCollider = collision.entity.getComponent<CollisionComponent>()->collider;
								RenderComponent* render = collision.entity.getComponent<RenderComponent>();
								if (ResolveDynamicRectVsRect(&rigidBody, &collider, &otherCollider)) {
									if (render) {
										render->color = { 255, 0, 255, 255 };
									}
								} else {
									if (render) {
										render->color = { 255, 0, 0, 255 };
									}
								}
							}
						}
					}
				}
				collider.position += rigidBody.velocity;
				transform.position = collider.position;
				/*
				if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_Q]) {
				} else {
					Game::aabbs.push_back({ collider, { 0, 0, 255, 255 } });
				}
				*/
			}
		}
	}
	AABB getSweptBroadphaseBox(const Vec2D& velocity, const AABB& b) {
		AABB broadphasebox;
		broadphasebox.position.x = velocity.x > 0 ? b.position.x : b.position.x + velocity.x;
		broadphasebox.position.y = velocity.y > 0 ? b.position.y : b.position.y + velocity.y;
		broadphasebox.size.x = velocity.x > 0 ? velocity.x + b.size.x : b.size.x - velocity.x;
		broadphasebox.size.y = velocity.y > 0 ? velocity.y + b.size.y : b.size.y - velocity.y;
		return broadphasebox;
	}
	bool PointvsAABB(const Vec2D& point, const AABB& a) {
		return (point.x >= a.position.x && 
				point.y >= a.position.y &&
				point.x < a.position.x + a.size.x &&
				point.y < a.position.y + a.size.y);
	}
	bool AABBvsAABB(const AABB& a, const AABB& b) {
		if (a.position.x + a.size.x < b.position.x || a.position.x > b.position.x + b.size.x) return false;
		if (a.position.y + a.size.y < b.position.y || a.position.y > b.position.y + b.size.y) return false;
		return true;
	}
	bool RayVsRect(const Vec2D& ray_origin, const Vec2D& ray_dir, const AABB* target, CollisionManifold& collision) {
		collision.normal = { 0,0 };
		collision.point = { 0,0 };

		// Cache division
		Vec2D invdir = 1 / ray_dir;
		// Calculate intersections with rectangle bounding axes
		Vec2D t_near = (target->position - ray_origin) * invdir;
		Vec2D t_far = (target->position + target->size - ray_origin) * invdir;

		if (std::isnan(t_far.y) || std::isnan(t_far.x)) return false;
		if (std::isnan(t_near.y) || std::isnan(t_near.x)) return false;

		// Sort distances
		if (t_near.x > t_far.x) std::swap(t_near.x, t_far.x);
		if (t_near.y > t_far.y) std::swap(t_near.y, t_far.y);

		// Early rejection		
		if (t_near.x > t_far.y || t_near.y > t_far.x) return false;

		// Closest 'time' will be the first contact
		collision.time = std::max(t_near.x, t_near.y);

		// Furthest 'time' is contact on opposite side of target
		double t_hit_far = std::min(t_far.x, t_far.y);

		// Reject if ray direction is pointing away from object
		if (t_hit_far < 0)
			return false;

		// Contact point of collision from parametric line equation
		collision.point = ray_origin + collision.time * ray_dir;

		if (t_near.x > t_near.y)
			if (invdir.x < 0)
				collision.normal = { 1, 0 };
			else
				collision.normal = { -1, 0 };
		else if (t_near.x < t_near.y)
			if (invdir.y < 0)
				collision.normal = { 0, 1 };
			else
				collision.normal = { 0, -1 };

		// Note if t_near == t_far, collision is principly in a diagonal
		// so pointless to resolve. By returning a CN={0,0} even though its
		// considered a hit, the resolver wont change anything.
		return true;
	}
	bool DynamicRectVsRect(const RigidBody* v_dynamic, const AABB* r_dynamic, const AABB& r_static,
						   CollisionManifold& collision) {
		// Check if dynamic rectangle is actually moving - we assume rectangles are NOT in collision to start
		if (v_dynamic->velocity.isZero()) return false;

		// Expand target rectangle by source dimensions
		AABB expanded_target;
		expanded_target.position = r_static.position - r_dynamic->size / 2.0;
		expanded_target.size = r_static.size + r_dynamic->size;
		//Game::aabbs.push_back(expanded_target);

		if (RayVsRect(r_dynamic->center(), v_dynamic->velocity, &expanded_target, collision)) {
			return collision.time >= 0.0 && collision.time < 1.0;
		} else {
			return false;
		}
	}
	bool ResolveDynamicRectVsRect(RigidBody* v_dynamic, const AABB* r_dynamic, AABB* r_static) {
		CollisionManifold collision;
		if (DynamicRectVsRect(v_dynamic, r_dynamic, *r_static, collision)) {
			v_dynamic->velocity += collision.normal * abs(v_dynamic->velocity) * (1.0 - collision.time);
			return true;
		}

		return false;
	}
	bool CirclevsCircle(Circle a, Circle b) {
		double r = a.radius + b.radius;
		r *= r;
		return r < (a.position + b.position).magnitudeSquared();
	}
	/*
	bool CirclevsCircle(CollisionManifold<Circle, Circle>& m) {
		// Setup a couple pointers to each object
		Circle& A = m.sA;
		Circle& B = m.sB;

		// Vector from A to B
		Vec2D n = B.position - A.position;

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
			m.normal = Vec2D(1.0, 0.0);
			return true;
		}
	}
	bool AABBvsCircle(CollisionManifold<AABB, Circle>& m) {
		// Setup a couple pointers to each object
		AABB& A = m.sA;
		Circle& B = m.sB;
		// Vector from A to B
		Vec2D n = B.position - A.position;
		// Closest point on A to center of B
		Vec2D closest = n;
		// Clamp point to edges of the AABB
		closest.x = Util::clamp(-A.size.x / 2.0, A.size.x / 2.0, closest.x);
		closest.y = Util::clamp(-A.size.y / 2.0, A.size.y / 2, closest.y);
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

		Vec2D normal = n - closest;
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
	AABB minkowskiDifference(const AABB& a, const AABB& b) {
		return AABB(a.position - b.position + b.size, a.size + b.size);
	}
};