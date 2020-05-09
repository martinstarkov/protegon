#include "Player.h"
#include "defines.h"

Player* Player::instance = nullptr;

#define MOVEMENT_ACCELERATION 1.0f
#define JUMPING_ACCELERATION 3.8f//3.8f

void Player::init() {
	hitbox = { Vec2D(232, 10), Vec2D(32, 32) };
	originalPos = hitbox.pos;
	velocity = {};
	acceleration = {};
	hasGravity = true;
}

void Player::update() {
	updateMotion();
	collisionCheck();

	groundCheck();
	boundaryCheck();

	clearColliders();
}

void Player::groundCheck() {
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