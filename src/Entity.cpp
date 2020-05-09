#include "Entity.h"
#include "Game.h"
#include <algorithm>
#include <limits>

#define GRAVITY 0.2f//0.2f
#define DRAG 0.2f

void Entity::update() {
	updateMotion();
	collisionCheck();
	clearColliders();
}

void Entity::updateMotion() {
	if (hasGravity) {
		acceleration.y += GRAVITY;
	}
	velocity *= (1.0f - DRAG); // drag
	velocity += acceleration; // movement
}

void Entity::collisionCheck() {
	AABB bpb = hitbox.broadphaseBox(hitbox.pos + velocity);
	Game::broadphase.push_back(bpb);

	std::vector<Entity*> potentialColliders;

	for (Entity* entity : Game::entities) {
		if (entity != this) {
			if (broadphaseCheck(bpb, entity)) {
				potentialColliders.push_back(entity);
			}
		}
	}

	hitbox.pos.x += velocity.x;

	for (Entity* entity : potentialColliders) {
		AABB md = hitbox.minkowskiDifference(entity->getHitbox());
		if (md.pos.x <= 0 &&
			md.getMax().x >= 0 &&
			md.pos.y <= 0 &&
			md.getMax().y >= 0) {

			Vec2D edge;
			Vec2D pv;
			std::cout << "p1:" << hitbox.pos << ",p2:" << entity->getHitbox().pos;
			md.penetrationVector(Vec2D(), pv, edge, velocity);
			hitbox.pos.x -= pv.x;
		}
	}
	hitbox.pos.y += velocity.y;

	for (Entity* entity : potentialColliders) {
		AABB md = hitbox.minkowskiDifference(entity->getHitbox());
		if (md.pos.x <= 0 &&
			md.getMax().x >= 0 &&
			md.pos.y <= 0 &&
			md.getMax().y >= 0) {

			Vec2D edge;
			Vec2D pv;
			md.penetrationVector(Vec2D(), pv, edge, velocity);
			yCollisions.push_back(pv);
			hitbox.pos.y -= pv.y;
		}
	}
}

void Entity::clearColliders() {
	yCollisions.clear();
}

bool Entity::isColliding(Entity* entity) {
	if (
		entity->getHitbox().pos.x + entity->getHitbox().size.x >= hitbox.pos.x &&
		entity->getHitbox().pos.x <= hitbox.pos.x + hitbox.size.x &&
		entity->getHitbox().pos.y + entity->getHitbox().size.y >= hitbox.pos.y &&
		entity->getHitbox().pos.y <= hitbox.pos.y + hitbox.size.y
		) { // entities collide
		return true;
	}
	return false;
}

void Entity::stop(Axis axis) {
	switch (axis) {
		case VERTICAL:
			acceleration.y = 0.0f;
			break;
		case HORIZONTAL:
			acceleration.x = 0.0f;
			break;
		case BOTH:
			acceleration = {};
		default:
			break;
	}
}

bool Entity::broadphaseCheck(AABB bpb, Entity* entity) {
	if (
		entity->getHitbox().pos.x + entity->getHitbox().size.x >= bpb.pos.x &&
		entity->getHitbox().pos.x <= bpb.pos.x + bpb.size.x &&
		entity->getHitbox().pos.y + entity->getHitbox().size.y >= bpb.pos.y &&
		entity->getHitbox().pos.y <= bpb.pos.y + bpb.size.y
		) { // entity collides with broadphase box
		return true;
	}
	return false;
}