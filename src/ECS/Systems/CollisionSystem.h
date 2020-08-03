#pragma once

#include "System.h"

#include "../../Game.h"

#include <algorithm>

// TODO: Redo entire collision system

struct CollisionInformation {
	Vec2D normal;
	Vec2D point;
	double nearHitTime;
	CollisionInformation() : nearHitTime(0.0) {}
};

template <typename T>
struct DynamicObject {
	RigidBody& rigidBody;
	const T& shape;
	DynamicObject(RigidBody& rigidBody, T& shape) : shape(shape), rigidBody(rigidBody) {}
};

struct Collision {
	CollisionInformation collision;
	Entity contactEntity;
	AABB* collider;
};

class CollisionSystem : public System<TransformComponent, RigidBodyComponent, CollisionComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			Vec2D& position = e.getComponent<TransformComponent>()->position;
			AABB& collider = e.getComponent<CollisionComponent>()->collider;
			collider.position = Vec2D(position);
		}
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			RigidBody& rigidBody = e.getComponent<RigidBodyComponent>()->rigidBody;
			Vec2D& position = e.getComponent<TransformComponent>()->position;
			AABB& collider = e.getComponent<CollisionComponent>()->collider;
			RenderComponent* render = e.getComponent<RenderComponent>();
			PlayerController* player = e.getComponent<PlayerController>();
			if (player) {
				// Sort collisions in order of distance
				CollisionInformation collision;
				std::vector<std::pair<AABB, double>> z;
				// Work out collision point, add it to vector along with rect ID
				for (auto& oId : entities) {
					if (oId != id) {
						Entity o = Entity(oId, manager);
						AABB& otherCollider = o.getComponent<CollisionComponent>()->collider;
						if (DynamicAABBvsAABB(DynamicObject<AABB>{ rigidBody, collider }, otherCollider, collision)) {
							z.push_back({ otherCollider, collision.nearHitTime });
						}
					}
				}
				// Do the sort
				std::sort(z.begin(), z.end(), [](const std::pair<AABB, double>& a, const std::pair<AABB, double>& b) {
					return a.second < b.second;
				});

				// Now resolve the collision in correct order 
				for (auto j : z) {
					LOG("Collision time: " << j.second);
					if (ResolveDynamicAABBvsAABB(DynamicObject<AABB>{ rigidBody, collider }, j.first, collision)) {
						// TODO: This only resolves the first collision in both directions, then ignores x and goes through wall
					}
				}

				// UPdate the player rectangles position, with its modified velocity
				position += rigidBody.velocity;
			}
		}
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
	bool RayvsAABB(const Ray2D& ray, const AABB& target, CollisionInformation& collision) {
		//Game::lines.push_back({ ray.origin, ray.origin + ray.direction });
		collision.normal = { 0,0 };
		collision.point = { 0,0 };
		collision.nearHitTime = 0.0;
		// Cache division
		Vec2D invdir = 1.0 / ray.direction;

		// Calculate intersections with rectangle bounding axes
		Vec2D t_near = (target.position - ray.origin) * invdir;
		Vec2D t_far = (target.position + target.size - ray.origin) * invdir;

		if (std::isnan(t_far.y) || std::isnan(t_far.x)) return false;
		if (std::isnan(t_near.y) || std::isnan(t_near.x)) return false;

		// Sort distances
		if (t_near.x > t_far.x) std::swap(t_near.x, t_far.x);
		if (t_near.y > t_far.y) std::swap(t_near.y, t_far.y);

		// Early rejection		
		if (t_near.x > t_far.y || t_near.y > t_far.x) return false;

		// Closest 'time' will be the first contact
		collision.nearHitTime = std::max(t_near.x, t_near.y);

		// Furthest 'time' is contact on opposite side of target
		double t_hit_far = std::min(t_far.x, t_far.y);

		// Reject if ray direction is pointing away from object
		if (t_hit_far < 0)
			return false;

		// Contact point of collision from parametric line equation
		collision.point = ray.origin + collision.nearHitTime * ray.direction;

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
	bool DynamicAABBvsAABB(const DynamicObject<AABB>& dynamicA, const AABB& staticB, CollisionInformation& collision) {
		// Check if dynamic rectangle is actually moving - we assume rectangles are NOT in collision to start
		if (dynamicA.rigidBody.velocity.isZero()) return false;

		// Expand target rectangle by source dimensions
		AABB expanded = staticB.expandedBy(dynamicA.shape);
		//Game::aabbs.push_back(expanded);

		if (RayvsAABB(Ray2D(dynamicA.shape.position + dynamicA.shape.size / 2.0, dynamicA.rigidBody.velocity), expanded, collision)) {
			Game::points.push_back(collision.point);
			return (collision.nearHitTime >= 0.0 && collision.nearHitTime < 1.0);
		} else {
			return false;
		}
	}
	bool ResolveDynamicAABBvsAABB(const DynamicObject<AABB>& dynamicA, const AABB& staticB, CollisionInformation& collision) {
		collision.nearHitTime = 0.0;
		collision.normal = { 0, 0 };
		collision.point = { 0, 0 };
		if (DynamicAABBvsAABB(dynamicA, staticB, collision)) {
			LOG("Adding " << collision.normal * abs(dynamicA.rigidBody.velocity) * (1.0 - collision.nearHitTime) << " to velocity: " << dynamicA.rigidBody.velocity);
			dynamicA.rigidBody.velocity += collision.normal * abs(dynamicA.rigidBody.velocity) * (1.0 - collision.nearHitTime);
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