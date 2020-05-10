#include "Entity.h"
#include "Game.h"
#include <algorithm>
#include <limits>

#define GRAVITY 0.2f
#define DRAG 0.2f

void Entity::update() {
	updateMotion();
	collisionCheck();
	boundaryCheck();
}

void Entity::updateMotion() {
	if (gravity) {
		acceleration.y += g;
	}
	velocity *= (1.0f - DRAG); // drag
	velocity += acceleration; // movement
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

void Entity::collisionCheck() {
	AABB bpb = hitbox.broadphaseBox(hitbox.pos + velocity);
	Game::broadphase.push_back(bpb);

	std::vector<Entity*> potentialColliders;

	for (Entity* entity : Game::entities) {
		if (entity != this) {
			if (bpb.colliding(entity->getHitbox())) {
				potentialColliders.push_back(entity);
			}
		}
	}

	hitbox.pos.x += velocity.x;

	for (Entity* entity : potentialColliders) {
		AABB md = hitbox.minkowskiDifference(entity->getHitbox());
		if (md.pos.x < 0 &&
			md.getMax().x > 0 &&
			md.pos.y < 0 &&
			md.getMax().y > 0) {

			Vec2D edge;
			Vec2D pv;
			md.penetrationVector(Vec2D(), pv, edge, velocity);
			xCollisions.push_back({ entity, pv });
			hitbox.pos.x -= pv.x;
		}
	}
	hitbox.pos.y += velocity.y;

	for (Entity* entity : potentialColliders) {
		AABB md = hitbox.minkowskiDifference(entity->getHitbox());
		if (md.pos.x < 0 &&
			md.getMax().x > 0 &&
			md.pos.y < 0 &&
			md.getMax().y > 0) {

			Vec2D edge;
			Vec2D pv;
			md.penetrationVector(Vec2D(), pv, edge, velocity);
			yCollisions.push_back({ entity, pv });
			hitbox.pos.y -= pv.y;
		}
	}
	resolveCollision();
	clearColliders();
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
}