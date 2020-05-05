#include "Player.h"

Player* Player::instance = nullptr;

void Player::init() {
	setSize(Vec2D(32, 32));
	setPosition(Vec2D(128, 128));
	maxVel = Vec2D(8, 20);
}

void Player::update() {
	Vec2D oldVel = getVelocity();
	float dragCoefficient = 0.1f;
	float gravity = 0.1f;//1.0f;
	setVelocity(oldVel + getAcceleration());
	if (abs(getVelocity().x) > maxVel.x) {
		setVelocity(Vec2D(oldVel.x, getVelocity().y));
	}
	if (abs(getVelocity().y) > maxVel.y) {
		setVelocity(Vec2D(getVelocity().x, oldVel.y));
	}
	setVelocity(Vec2D(getVelocity().x, getVelocity().y + gravity));
	setVelocity(getVelocity() * (1.0f - dragCoefficient));
	setPosition(getPosition() + getVelocity());
	std::cout << "Pos: " << getPosition() << ", Vel: " << getVelocity() << ", Accel: " << getAcceleration() << std::endl;
}
