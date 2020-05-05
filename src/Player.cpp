#include "Player.h"
#include "Game.h"
#include <algorithm>

Player* Player::instance = nullptr;

void Player::init() {
	setSize(Vec2D(32, 32));
	setPosition(Vec2D(128, 128));
	maxVel = Vec2D(8, 8);//Vec2D(8, 20);
}

void Player::update() {
	Entity::update();
	Vec2D oldVel = getVelocity();
	float dragCoefficient = 0.1f;
	float gravity = 1.0f;//0.1f;//1.0f;
	setVelocity(getVelocity() * (1.0f - dragCoefficient));
	setVelocity(getVelocity() + getAcceleration());
	setPosition(getPosition() + getVelocity());
	//setVelocity(Vec2D(getVelocity().x, getVelocity().y + gravity));
	Vec2D normal = Vec2D();
	float collisiontime = sweptAABB(Game::entities[0], normal);
	//float remainingtime = 1.0f - collisiontime;
	if (collisiontime < 1) {
		if (normal.y != 0) {
			setAcceleration(Vec2D(getAcceleration().x, 0));
			setVelocity(Vec2D(getVelocity().x, 0));
		}
		if (normal.x != 0) {
			setAcceleration(Vec2D(0, getAcceleration().y));
			setVelocity(Vec2D(0, getVelocity().y));
		}
	}
	//if (abs(getVelocity().x) > maxVel.x) {
	//	setVelocity(Vec2D(getVelocity().unitVector().x * maxVel.x, getVelocity().y));
	//}
	//if (abs(getVelocity().y) > maxVel.y) {
	//	setVelocity(Vec2D(getVelocity().x, getVelocity().unitVector().y * maxVel.y));
	//}
	if (normal.y == -1) {
		jumping = false;
	}
	setPosition(getPosition() + normal * getVelocity() * (1.0f - collisiontime));
	std::cout << "Pos: " << getPosition() << ", Vel: " << getVelocity() << ", Accel: " << getAcceleration() << ",Normal: " << normal << std::endl;
}