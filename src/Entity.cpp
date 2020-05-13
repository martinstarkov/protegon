#include "Entity.h"
#include "Game.h"
#include <algorithm>
#include <limits>

void Entity::update() {
	updateMotion();
	collisionCheck();
	boundaryCheck();
}

void Entity::updateMotion() {
	if (gravity) {
		acceleration.y += g; // gravity
	}
	velocity *= (1.0f - DRAG); // drag
	velocity += acceleration;
	terminalMotion();
}

void Entity::terminalMotion() {
	if (velocity.x > terminalVelocity.x) {
		velocity.x = terminalVelocity.x;
	}
	if (velocity.y > terminalVelocity.y) {
		velocity.y = terminalVelocity.y;
	}
}

void Entity::boundaryCheck() {
	if (hitbox.pos.x < 0) {
		hitbox.pos.x = 0;
	}
	if (hitbox.pos.x + hitbox.size.x > WINDOW_WIDTH) {
		hitbox.pos.x = WINDOW_WIDTH - hitbox.size.x;
	}
	if (hitbox.pos.y < 0) {
		hitbox.pos.y = 0;
		velocity.y *= -1 / 2;
		acceleration.y *= -1 / 10;
	}
	if (hitbox.pos.y + hitbox.size.y > WINDOW_HEIGHT) {
		hitbox.pos.y = WINDOW_HEIGHT - hitbox.size.y;
		hitGround();
	}
}


AABB Entity::broadphaseBox(AABB a, Vec2D vel) {
	Vec2D bPos;
	bPos.x = vel.x > 0 ? a.pos.x : a.pos.x + vel.x;
	bPos.y = vel.y > 0 ? a.pos.y : a.pos.y + vel.y;
	Vec2D bSize = a.size + abs(vel);
	return AABB(bPos, bSize);
}

int Entity::TestAABBAABB(AABB a, AABB b) {
	// Exit with no intersection if separated along an axis
	if (a.max()[0] < b.min()[0] || a.min()[0] > b.max()[0]) return 0;
	if (a.max()[1] < b.min()[1] || a.min()[1] > b.max()[1]) return 0;
	// Overlapping on all axes means AABBs are intersecting
	return 1;
}
// Intersect AABBs ‘a’ and ‘b’ moving with constant velocities va and vb.
// On intersection, return time of first and last contact in tfirst and tlast
// Intersect AABBs ‘a’ and ‘b’ moving with constant velocities va and vb.
// On intersection, return time of first and last contact in tfirst and tlast
int Entity::IntersectMovingAABBAABB(AABB a, AABB b, Vec2D va, Vec2D vb,
									float& tfirst, float& tlast) {
	// Exit early if ‘a’ and ‘b’ initially overlapping
	if (TestAABBAABB(a, b)) {
		tfirst = tlast = 0.0f;
		return 1;
	}
	// Use relative velocity; effectively treating ’a’ as stationary
	Vec2D v = vb - va;
	// Initialize times of first and last contact
	tfirst = 0.0f;
	tlast = 1.0f;
	// For each axis, determine times of first and last contact, if any
	for (int i = 0; i < 2; i++) {
		if (v[i] < 0.0f) {
			if (b.max()[i] < a.min()[i]) return 0; // Nonintersecting and moving apart
			if (a.max()[i] < b.min()[i]) tfirst = std::max((a.max()[i] - b.min()[i]) / v[i], tfirst);
			if (b.max()[i] > a.min()[i]) tlast = std::min((a.min()[i] - b.max()[i]) / v[i], tlast);
		}
		if (v[i] > 0.0f) {
			if (b.min()[i] > a.max()[i]) return 0; // Nonintersecting and moving apart
			if (b.max()[i] < a.min()[i]) tfirst = std::max((a.min()[i] - b.max()[i]) / v[i], tfirst);
			if (a.max()[i] > b.min()[i]) tlast = std::min((a.max()[i] - b.min()[i]) / v[i], tlast);
		}
		// No overlap possible if time of first contact occurs after time of last contact
		if (tfirst > tlast) return 0;
	}
	return 1;
}

void Entity::collisionCheck() {
	AABB newHitbox = hitbox;
	std::vector<Entity*> statics;
	std::vector<float> sweepTimes;
	for (Entity* e : Game::entities) {
		if (e != this) {
			if (TestAABBAABB(broadphaseBox(hitbox, velocity), e->getHitbox())) {
				float entryTime, exitTime;
				int collision = IntersectMovingAABBAABB(hitbox, e->getHitbox(), velocity, e->getVelocity(), entryTime, exitTime);
				sweepTimes.push_back(entryTime);
				statics.push_back(e);
				//std::cout << "[" << collision << "]entry:" << entryTime << ",exit:" << exitTime << std::endl;
				collision = true;
			}
		}
	}
	auto it = std::min_element(sweepTimes.begin(), sweepTimes.end());
	float collisionTime = 0.0f;
	if (it != sweepTimes.end()) {
		collisionTime = *it;
	}
	if (collisionTime) {
		//std::cout << "Sweep" << std::endl;
		newHitbox.pos += velocity * collisionTime;
		velocity = Vec2D();
		if (!id) {
			std::cout << collisionTime << std::endl;
		}
	} else {

		AABB bpb = broadphaseBox(newHitbox, velocity);
		//Game::broadphase.push_back(bpb);

		std::vector<Entity*> potentialColliders;

		for (Entity* entity : statics) {
			if (entity != this) {
				if (TestAABBAABB(bpb, entity->getHitbox())) {
					potentialColliders.push_back(entity);
				}
			}
		}

		newHitbox.pos.x += velocity.x;

		for (Entity* entity : potentialColliders) {
			AABB md = newHitbox.minkowskiDifference(entity->getHitbox());
			if (md.pos.x < 0 &&
				md.max().x > 0 &&
				md.pos.y < 0 &&
				md.max().y > 0) {

				Vec2D edge;
				Vec2D pv;
				md.penetrationVector(Vec2D(), pv, edge, velocity);
				xCollisions.push_back({ entity, pv });
				newHitbox.pos.x -= pv.x;
			}
		}
		newHitbox.pos.y += velocity.y;

		for (Entity* entity : potentialColliders) {
			AABB md = newHitbox.minkowskiDifference(entity->getHitbox());
			if (md.pos.x < 0 &&
				md.max().x > 0 &&
				md.pos.y < 0 &&
				md.max().y > 0) {

				Vec2D edge;
				Vec2D pv;
				md.penetrationVector(Vec2D(), pv, edge, velocity);
				yCollisions.push_back({ entity, pv });
				newHitbox.pos.y -= pv.y;
			}
		}
	}
	resolveCollision();
	clearColliders();
	hitbox = newHitbox;
}

void Entity::clearColliders() {
	yCollisions.clear();
	xCollisions.clear();
}

void Entity::resolveCollision() {
	grounded = false;
	if (collided(Side::BOTTOM)) {
		hitGround();
	} else if (collided(Side::TOP)) {
		velocity.y *= -1 / 2;
		acceleration.y *= -1 / 10;
	}
}

void Entity::hitGround() {
	grounded = true;
	velocity.y = 0;
	acceleration.y = 0;
}

Entity* Entity::collided(Side direction) {
	switch (direction) {
		case Side::BOTTOM:
		case Side::TOP:
			if (yCollisions.size() > 0) {
				for (auto c : yCollisions) {
					if (c.second.y > 0 && direction == Side::BOTTOM) {
						return c.first;
					}
					if (c.second.y < 0 && direction == Side::TOP) {
						return c.first;
					}
				}
			}
			break;
		case Side::LEFT:
		case Side::RIGHT:
			if (xCollisions.size() > 0) {
				for (auto c : xCollisions) {
					if (c.second.x > 0 && direction == Side::RIGHT) {
						return c.first;
					}
					if (c.second.x < 0 && direction == Side::LEFT) {
						return c.first;
					}
				}
			}
			break;
		case Side::ANY:
			if (yCollisions.size() > 0) {
				return yCollisions[0].first;
			}
			if (xCollisions.size() > 0) {
				return xCollisions[0].first;
			}
		default:
			break;
	}
	return nullptr;
}

void Entity::reset() {
	acceleration = velocity = {};
	hitbox.pos = originalPos;
	gravity = falling;
	g = 0.2f;
	color = originalColor;
}

void Entity::accelerate(Axis direction, float movementAccel) {
	switch (direction) {
		case Axis::VERTICAL:
			acceleration.y = movementAccel;
			break;
		case Axis::HORIZONTAL:
			acceleration.x = movementAccel;
			break;
		case Axis::BOTH:
			acceleration = Vec2D(movementAccel, movementAccel);
		default:
			break;
	}
}

void Entity::stop(Axis direction) {
	accelerate(direction, 0.0f);
	//switch (direction) {
	//	case Axis::VERTICAL:
	//		velocity.y = 0.0f;
	//		break;
	//	case Axis::HORIZONTAL:
	//		velocity.x = 0.0f;
	//		break;
	//	case Axis::BOTH:
	//		velocity = Vec2D();
	//	default:
	//		break;
	//}
}