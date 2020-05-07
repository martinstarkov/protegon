#include "Player.h"
#include "Game.h"
#include <algorithm>

Player* Player::instance = nullptr;
#define GRAVITY 0.1f//0.01f
#define DRAG 0.25f

void Player::init() {
	hitbox = { Vec2D(128, 128), Vec2D(32, 32) };
	velocity = {};
	acceleration = {};
	//setSize(Vec2D(32, 32));
	//setPosition(Vec2D(128, 128));
	//maxVel = Vec2D(8, 8);//Vec2D(8, 20);
}

void Player::collisionCheck(Entity* entity) {
	AABB md = hitbox.minkowskiDifference(entity->getHitbox());
	if (md.pos.x <= 0 &&
		md.getMax().x >= 0 &&
		md.pos.y <= 0 &&
		md.getMax().y >= 0) {
		Vec2D pv = Vec2D();
		Vec2D edge = Vec2D();
		md.penetrationVector(Vec2D(0, 0), pv, edge); // relative to origin
		if (edge.y == 1) {
			jumping = false;
			velocity.y = 0;
			acceleration.y = 0;
		}
		hitbox.pos -= pv;
	}
}

void Player::update() {
	Entity::update();
	acceleration.y += GRAVITY;
	velocity *= (1.0f - DRAG); // drag
	velocity += acceleration; // movement
	hitbox.pos += velocity;
	for (Entity* entity : Game::entities) {
		collisionCheck(entity);
	}
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