#include "Player.h"
#include "Game.h"
#include <algorithm>

Player* Player::instance = nullptr;
#define GRAVITY 0.2f//0.2f
#define DRAG 0.2f
#define MOVEMENT_ACCELERATION 1.0f
#define JUMPING_ACCELERATION 3.8f//3.8f

void Player::init() {
	hitbox = { Vec2D(232, 10), Vec2D(32, 32) };
	oldHitbox = hitbox;
	originalPos = hitbox.pos;
	velocity = {};
	acceleration = {};
	//setSize(Vec2D(32, 32));
	//setPosition(Vec2D(128, 128));
	//maxVel = Vec2D(8, 8);//Vec2D(8, 20);
}

void Player::hitGround() {
	grounded = true;
	jumping = false;
	velocity.y = 0;
	acceleration.y = 0;
	//std::cout << "Hit the ground!" << std::endl;
}

void Player::resetPosition() {
	stop(BOTH);
	hitbox.pos = originalPos;
}

void Player::boundaryCheck() {
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

void Player::move(Keys key) {
	switch (key) {
		case LEFT:
			acceleration.x = -MOVEMENT_ACCELERATION;
			break;
		case RIGHT:
			acceleration.x = MOVEMENT_ACCELERATION;
			break;
		case UP:
			if (!jumping) {
				jumping = true;
				acceleration.y = -JUMPING_ACCELERATION;
			}
			break;
		case DOWN:
			acceleration.y += MOVEMENT_ACCELERATION / 100; // slightly increase downward acceleration
			break;
		default:
			break;
	}
}

void Player::stop(Axis axis) {
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

bool Player::broadPhaseCheck(AABB bpb, Entity* entity) {
	if (
		entity->getHitbox().pos.x + entity->getHitbox().size.x >= bpb.pos.x &&
		entity->getHitbox().pos.x <= bpb.pos.x + bpb.size.x &&
		entity->getHitbox().pos.y + entity->getHitbox().size.y >= bpb.pos.y &&
		entity->getHitbox().pos.y <= bpb.pos.y + bpb.size.y
		) { // entity collides with broadphase box
		//std::cout << "Broadphase collision" << std::endl;
		return true;
	}
	return false;
}

bool Player::collisionCheck(Entity* entity) {
	if (
		entity->getHitbox().pos.x + entity->getHitbox().size.x > hitbox.pos.x &&
		entity->getHitbox().pos.x < hitbox.pos.x + hitbox.size.x &&
		entity->getHitbox().pos.y + entity->getHitbox().size.y > hitbox.pos.y &&
		entity->getHitbox().pos.y < hitbox.pos.y + hitbox.size.y
		) { // entity collides with player
		return true;
	}
	return false;
}

void Player::update() {

	oldHitbox = hitbox;

	Entity::update();

	acceleration.y += GRAVITY;
	velocity *= (1.0f - DRAG); // drag
	velocity += acceleration; // movement

	AABB bpb = hitbox.broadphaseBox(hitbox.pos + velocity);
	Game::broadphase.push_back(bpb);

	std::vector<Entity*> potentialColliders;

	for (Entity* entity : Game::entities) {
		if (broadPhaseCheck(bpb, entity)) {
			potentialColliders.push_back(entity);
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
			md.penetrationVector(Vec2D(), pv, edge, velocity);

			hitbox.pos.x -= pv.x;

		}

	}

	hitbox.pos.y += velocity.y;

	std::vector<Vec2D> yCollisions;

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

	grounded = false;
	jumping = true;

	if (velocity.y != 0) {
		if (yCollisions.size() > 0) {
			for (auto& pV : yCollisions) {
				if (pV.y > 0) {
					hitGround();
					break;
				}
				if (pV.y < 0) {
					velocity.y *= -1 / 2;
					acceleration.y *= -1 / 10;
				}
			}
		}
	}
	
	boundaryCheck();

	//std::cout << "Vel: " << velocity << std::endl;

	//setPosition(getPosition() + getVelocity());
	////setVelocity(Vec2D(getVelocity().x, getVelocity().y + gravity));
	//Vec2D normal = Vec2D();
	//float collisiontime = sweptAABB(Game::entities[0], normal);
	////float remainingtime = 1.0f - collisiontime;
	//if (collisiontime < 1) {
	//	if (normal.y != 0) {
	//		setAcceleration(Vec2D(getAcceleration().x, 0));
	//		setVelocity(Vec2D(getVelocity().x, 0));
	//	}
	//	if (normal.x != 0) {
	//		setAcceleration(Vec2D(0, getAcceleration().y));
	//		setVelocity(Vec2D(0, getVelocity().y));
	//	}
	//}
	//if (abs(getVelocity().x) > maxVel.x) {
	//	setVelocity(Vec2D(getVelocity().unitVector().x * maxVel.x, getVelocity().y));
	//}
	//if (abs(getVelocity().y) > maxVel.y) {
	//	setVelocity(Vec2D(getVelocity().x, getVelocity().unitVector().y * maxVel.y));
	//}
	//if (normal.y == -1) {
	//	jumping = false;
	//}
	//setPosition(getPosition() + normal * getVelocity() * (1.0f - collisiontime));
	//std::cout << "Pos: " << getPosition() << ", Vel: " << getVelocity() << ", Accel: " << getAcceleration() << ",Normal: " << normal << std::endl;
}