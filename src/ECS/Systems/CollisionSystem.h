#pragma once

#include "System.h"

#include <algorithm>

// TODO: Redo entire collision system

template <typename T, typename S>
struct CollisionManifold {
	RigidBody& rbA;
	RigidBody& rbB;
	T& sA;
	S& sB;
	double penetration;
	Vec2D normal;
	CollisionManifold(RigidBody& rbA, RigidBody& rbB, T& sA, S& sB) : rbA(rbA), rbB(rbB), sA(sA), sB(sB), penetration(0.0), normal() {}
};

class CollisionSystem : public System<TransformComponent, RigidBodyComponent, CollisionComponent> {
public:
	virtual void update() override final {
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			RigidBody& rigidBody = e.getComponent<RigidBodyComponent>()->rigidBody;
			Vec2D& position = e.getComponent<TransformComponent>()->position;
			AABB& collider = e.getComponent<CollisionComponent>()->collider;
			RenderComponent* render = e.getComponent<RenderComponent>();
			// keep collider synced with transform
			bool colliding = false;
			collider.position = Vec2D(position);
			for (auto& oId : entities) {
				if (oId != id) {
					Entity o = Entity(oId, manager);
					RigidBody& otherRigidBody = o.getComponent<RigidBodyComponent>()->rigidBody;
					Vec2D& otherPosition = o.getComponent<TransformComponent>()->position;
					AABB& otherCollider = o.getComponent<CollisionComponent>()->collider;
					// keep collider synced with transform
					otherCollider.position = Vec2D(otherPosition);
					CollisionManifold<AABB, AABB> m = CollisionManifold<AABB, AABB>(rigidBody, otherRigidBody, collider, otherCollider);
					if (AABBvsAABB(m.sA, m.sB)) {
						colliding = true;
						//AABBvsAABB(m);
						//LOG("Normal: " << m.normal << ", penetration: " << m.penetration);
						//resolveCollision(m);
						//correctPositions(m);
						//// update transform positions to match collider
						//otherPosition = otherCollider.position;
						//position = collider.position;
						////LOG("Collision occured");
					}
					if (e.hasComponent<PlayerController>()) {
						//LOG("vel: " << rigidBody.velocity << ", tvel: " << rigidBody.terminalVelocity << ", drag: " << rigidBody.drag << ", accel: " << rigidBody.acceleration << ", gravity: " << rigidBody.gravity);
					}
				}
			}
			if (colliding) {
				if (render) {
					render->color = { 255, 0, 0, 255 };
				}
			} else {
				if (render) {
					render->color = { 0, 0, 0, 255 };
				}
			}
		}
	}
	bool AABBvsAABB(AABB& a, AABB& b) {
		// Exit with no intersection if found separated along an axis
		if (a.max().x < b.min().x || a.min().x > b.max().x) return false;
		if (a.max().y < b.min().y || a.min().y > b.max().y) return false;
		// No separating axis found, therefor there is at least one overlapping axis
		return true;
	}
	bool AABBvsAABB(CollisionManifold<AABB, AABB>& m) {
		// Setup a couple pointers to each object
		AABB& A = m.sA;
		AABB& B = m.sB;
		// Vector from A to B
		Vec2D n = B.position - A.position;
		// Calculate overlap on x axis
		double xOverlap = A.size.x / 2.0 + B.size.x / 2.0 - abs(n.x);
		// SAT test on x axis
		if (xOverlap > 0.0) {
			// Calculate overlap on y axis
			double yOverlap = A.size.y / 2.0 + B.size.y / 2.0 - abs(n.y);
			// SAT test on y axis
			if (yOverlap > 0.0) {
				// Find out which axis is axis of least penetration
				if (xOverlap > yOverlap) {
					// Point towards B knowing that n points from A to B
					if (n.x < 0.0) {
						m.normal = Vec2D(-1.0, 0.0);
					} else {
						m.normal = Vec2D(1.0, 0.0);
						m.penetration = xOverlap;
						return true;
					}
				} else {
					// Point toward B knowing that n points from A to B
					if (n.y < 0.0) {
						m.normal = Vec2D(0.0, -1.0);
					} else {
						m.normal = Vec2D(0.0, 1.0);
						m.penetration = yOverlap;
						return true;
					}
				}
			}
		}
		return false;
	}
	bool CirclevsCircle(Circle a, Circle b) {
		double r = a.radius + b.radius;
		r *= r;
		return r < (a.position.x + b.position.x)* (a.position.x + b.position.x) + (a.position.y + b.position.y) * (a.position.y + b.position.y);
	}
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
	template <typename T, typename S>
	void resolveCollision(CollisionManifold<T, S>& m) {
		RigidBody& A = m.rbA;
		RigidBody& B = m.rbB;
		// Calculate relative velocity
		Vec2D rv = B.velocity - A.velocity;

		// Calculate relative velocity in terms of the normal direction
		double velAlongNormal = rv.dotProduct(m.normal);

		// Do not resolve if velocities are separating
		if (velAlongNormal > 0.0) {
			return;
		}

		// Calculate restitution
		double e = std::min(A.restitution, B.restitution);

			// Calculate impulse scalar
		double j = -(1 + e) * velAlongNormal;
		j /= A.inverseMass + B.inverseMass;

		// Apply impulse
		Vec2D impulse = j * m.normal;
		A.velocity -= A.inverseMass * impulse;
		B.velocity += B.inverseMass * impulse;
	}
	template <typename T, typename S>
	void correctPositions(CollisionManifold<T, S>& m) {
		Vec2D n = m.sB.position - m.sA.position;
		const double percent = 0.2; // usually 20% to 80%
		const double slop = 0.01; // usually 0.01 to 0.1
		Vec2D correction = std::max(m.penetration - slop, 0.0) / (m.rbA.inverseMass + m.rbB.inverseMass) * percent * n;
		m.sA.position -= m.rbA.inverseMass * correction;
		m.sB.position += m.rbB.inverseMass * correction;
	}
};