#pragma once

#include "System.h"

#include <Game.h>
#include <AABB.h>

#include <algorithm>
#include <vector>

// TODO: Use relative velocities for two dynamic blocks
// TODO: Figure out how to resolve out all static collision in one frame
// TODO: Optimize performance (reduce checks, reduce copying, improve broadphase, add dynamic tree or sort and sweep algorithm?)
// TODO: Revisit structured binding names
// BUG: Fix static clipping through left corner walls
// BUG / TODO: Fix wall glitch at very fast speeds

struct CollisionManifold {
	Vec2D point;
	Vec2D normal;
	double time = 0.0;
	friend std::ostream& operator<<(std::ostream& os, const CollisionManifold& obj) {
		os << "point: " << obj.point << ", normal: " << obj.normal << ", time: " << obj.time;
		return os;
	}
};

struct Collision {
	ecs::Entity id;
	CollisionManifold manifold;
};

static void sortTimes(std::vector<Collision>& collisions) {
	// Sort collision sweep times
	std::sort(collisions.begin(), collisions.end(), [](const Collision& a, const Collision& b) {
		if (a.manifold.time != b.manifold.time) {
			return a.manifold.time < b.manifold.time;
		} else {
			return a.manifold.normal.magnitudeSquared() < b.manifold.normal.magnitudeSquared();
		}
	});
}

static void sortDepths(std::vector<double>& depths) {
	// Sort collision depths
	std::sort(depths.begin(), depths.end(), [](const double& a, const double& b) {
		return a > b;
	});
}

// Function which takes consecutive identical collision times such as [0.5: (0, -1), 0.5: (-1, 0)] and combines the normals into one time [0.5: (-1, -1)]
static void combineTimes(std::vector<Collision>& collisions) {
	size_t size = collisions.size();
	if (size > 1) {
		std::vector<Collision> newCollisions;
		for (size_t i = 0; i < size; ++i) {
			static bool ignore = false;
			if (!ignore) {
				if (collisions[i].manifold.time == collisions[(i + 1) % size].manifold.time) {
					if (collisions[i].manifold.normal.hasZero() && collisions[(i + 1) % size].manifold.normal.hasZero()) {
						if (abs(collisions[i].manifold.normal.x) != abs(collisions[(i + 1) % size].manifold.normal.x)) {
							Collision col = collisions[i];
							col.manifold.normal += collisions[(i + 1) % size].manifold.normal;
							newCollisions.push_back(col);
							ignore = true;
							continue;
						}
					}
				}
				newCollisions.push_back(collisions[i]);
			} else {
				ignore = false;
			}
		}
		collisions = newCollisions;
	}
}

static void printCollisions(const std::vector<Collision>& collisions) {
	LOG_("Collisions: ");
	for (auto& collision : collisions) {
		LOG_("[" << collision.manifold.time << ": " << collision.manifold.normal << "], ");
	}
	LOG("");
}

class CollisionSystem : public ecs::System<TransformComponent, CollisionComponent> {
public:
	virtual void Update() override final {
		static int counter = 0;
		// sync collider positions to transform positions
		for (auto [entity, transform, collision] : entities) {
			collision.collider.position = transform.position;
			// Color reset
			if (!entity.HasComponent<PlayerController>()) {
				if (entity.HasComponent<RenderComponent>()) {
					entity.GetComponent<RenderComponent>().color = BLACK;
				}
			} else {
				if (entity.HasComponent<RenderComponent>()) {
					entity.GetComponent<RenderComponent>().color = BLUE;
				}
			}
			
		}
		std::vector<ecs::Entity> staticCheck;
		for (auto [entity, transform, collision] : entities) {
			if (entity.HasComponent<RigidBodyComponent>()) {
				RigidBody& rigidBody = entity.GetComponent<RigidBodyComponent>().rigidBody;
				AABB& collider = collision.collider;
				std::vector<Collision> collisions;
				std::vector<ecs::Entity> broadphaseEntities;
				AABB broadphase = getSweptBroadphaseBox(rigidBody.velocity, collider);
				//Game::aabbs.push_back({ broadphase, { 255, 0, 0, 255 } });
				// broad phase static check
				for (auto [entity2, transform2, collision2] : entities) {
					if (entity2 != entity) {
						AABB oCollider = collision2.collider;
						if (AABBvsAABB(broadphase, oCollider)) {
							broadphaseEntities.emplace_back(entity2);
						}
					}
				}
				Collision info;
				// narrow phase dynamic check
				for (auto& entity2 : broadphaseEntities) {
					AABB& oCollider = entity2.GetComponent<CollisionComponent>().collider;
					if (DynamicAABBvsAABB(&rigidBody, &collider, oCollider, info.manifold)) {
						info.id = entity2;
						collisions.push_back(info);
					}
				}
				sortTimes(collisions);
				//combineTimes(collisions);
				//printCollisions(collisions);
				// narrow phase dynamic resolution
				if (collisions.size() > 0) {
					Vec2D oldVelocity = rigidBody.velocity;
					for (auto& c : collisions) {
						AABB& oCollider = c.id.GetComponent<CollisionComponent>().collider;
						ResolveDynamicAABBvsAABB(&rigidBody, &collider, &oCollider, c.manifold);
					}
					/*AABB futureCollider = collider;
					AABB firstCollider = collider;
					futureCollider.position += oldVelocity;
					firstCollider.position += rigidBody.velocity;
					Game::aabbs.push_back({ futureCollider, RED });
					//LOG("Vel: " << oldVelocity << ", new Vel: " << rigidBody.velocity);
					Game::lines.push_back({ collider.center(), collider.center() + oldVelocity, RED });
					Game::lines.push_back({ collider.center(), collider.center() + rigidBody.velocity, ORANGE });
					*/
					// velocity changed, complete a second sweep
					if (rigidBody.velocity.x != oldVelocity.x || rigidBody.velocity.y != oldVelocity.y) {
						collisions.clear();
						// second narrow phase dynamic check
						for (auto& entity2 : broadphaseEntities) {
							AABB& oCollider = entity2.GetComponent<CollisionComponent>().collider;
							if (DynamicAABBvsAABB(&rigidBody, &collider, oCollider, info.manifold)) {
								info.id = entity2;
								collisions.push_back(info);
							}
						}
						// second narrow phase dynamic resolution
						if (collisions.size() > 0) {
							sortTimes(collisions);
							for (auto& c : collisions) {
								AABB& oCollider = c.id.GetComponent<CollisionComponent>().collider;
								ResolveDynamicAABBvsAABB(&rigidBody, &collider, &oCollider, c.manifold);
							}
						}
					}
				}
				collider.position += rigidBody.velocity;
				if (transform.position != collider.position) {
					transform.position = collider.position;
					staticCheck.push_back(entity);
				}
				/*if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_SPACE] && counter % 6 == 0) {
					transform->position = collider.position;
				} else {
					Game::aabbs.push_back({ collider, GREEN });
				}*/
			}
		}
		// Static collision resolution
		for (auto& entity : staticCheck) {
			auto [transform, collisionComponent] = entity.GetComponents<TransformComponent, CollisionComponent>();
			AABB& collider = collisionComponent.collider;
			for (auto [entity2, transform2, collision2] : entities) {
				if (entity2 == entity) continue;
				AABB& oCollider = collision2.collider;
				if (AABBvsAABB(collider, oCollider)) {
					Vec2D collisionDepth = intersectAABB(collider, oCollider);
					if (!collisionDepth.isZero()) {
						collider.position -= collisionDepth;
						transform.position = collider.position;
						if (entity.HasComponent<RenderComponent>()) {
							entity.GetComponent<RenderComponent>().color = RED;
						}
					}
				}
			}
		}
		++counter;
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
	Vec2D intersectAABB(AABB otherBox, AABB box) {
		Vec2D penetration;
		double dx = box.position.x - otherBox.position.x;
		double px = (box.size.x / 2 + otherBox.size.x / 2) - abs(dx);
		if (px <= 0) {
		  return penetration;
		}

		double dy = box.position.y - otherBox.position.y;
		double py = (box.size.y / 2 + otherBox.size.y / 2) - abs(dy);
		if (py <= 0) {
		  return penetration;
		}

		if (px < py) {
		  double sx = Util::sgn(dx);
		  penetration.x = px * sx;
		} else {
		  double sy = Util::sgn(dy);
		  penetration.y = py * sy;
		}
		return penetration;
	}

	bool AABBvsAABB(const AABB& a, const AABB& b) {
		if (a.position.x + a.size.x <= b.position.x || a.position.x >= b.position.x + b.size.x) return false;
		if (a.position.y + a.size.y <= b.position.y || a.position.y >= b.position.y + b.size.y) return false;
		return true;
	}
	bool RayvsAABB(const Vec2D& ray_origin, const Vec2D& ray_dir, const AABB* target, CollisionManifold& collision) {
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

		if (t_near.x > t_near.y) {
			if (invdir.x < 0) {
				collision.normal = { 1, 0 };
			} else {
				collision.normal = { -1, 0 };
			}
		} else if (t_near.x < t_near.y) {
			if (invdir.y < 0) {
				collision.normal = { 0, 1 };
			} else {
				collision.normal = { 0, -1 };
			}
		} else if (t_near.x == t_near.y && t_far.x == t_far.y) {
			// diagonal collision, set normal to opposite of velocity
			//LOG();
			collision.normal = ray_dir.identity().opposite();
		}
		// Note if t_near == t_far, collision is principly in a diagonal
		// so pointless to resolve. By returning a CN={0,0} even though its
		// considered a hit, the resolver wont change anything.
		//targetColor = BLUE;
		return true;
	}
	bool DynamicAABBvsAABB(const RigidBody* v_dynamic, const AABB* r_dynamic, const AABB& r_static,
						   CollisionManifold& collision) {
		// Check if dynamic rectangle is actually moving - we assume rectangles are NOT in collision to start
		if (v_dynamic->velocity.isZero()) return false;

		// Expand target rectangle by source dimensions
		AABB expanded_target;
		expanded_target.position = r_static.position - r_dynamic->size / 2.0;
		expanded_target.size = r_static.size + r_dynamic->size;
		//Game::aabbs.push_back({ expanded_target, SILVER });

		if (RayvsAABB(r_dynamic->center(), v_dynamic->velocity, &expanded_target, collision)) {
			return collision.time >= 0.0 && collision.time < 1.0;
		} else {
			return false;
		}
	}
	bool ResolveDynamicAABBvsAABB(RigidBody* v_dynamic, const AABB* r_dynamic, AABB* r_static, CollisionManifold collision) {
		CollisionManifold repeatCheck;
		if (DynamicAABBvsAABB(v_dynamic, r_dynamic, *r_static, repeatCheck)) {
			//LOG("v:" << v_dynamic->velocity << "+=" << collision.normal << "*abs(" << v_dynamic->velocity << ")*(" << (1.0 - collision.time) << ")");
			v_dynamic->velocity += collision.normal * abs(v_dynamic->velocity) * (1.0 - collision.time);
			/*Game::lines.push_back({ collision.point, collision.point + collision.normal * 10, PURPLE });
			AABB expanded_target;
			expanded_target.position = r_static->position - r_dynamic->size / 2.0;
			expanded_target.size = r_static->size + r_dynamic->size;
			Game::aabbs.push_back({ expanded_target, GREY });*/
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