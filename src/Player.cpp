#include "Player.h"
#include "defines.h"
#include "FallingPlatform.h"
#include "Game.h"

Player* Player::instance = nullptr;

#define MOVEMENT_ACCELERATION 1.0f
#define JUMPING_ACCELERATION 40.0f

void Player::init() {
	hitbox = { Vec2D(128 * 4, 128 * 4), Vec2D(32, 32) };
	id = PLAYER_ID;
	originalPos = hitbox.pos;
	velocity = {};
	acceleration = {};
	movementAcceleration = MOVEMENT_ACCELERATION;
	jumpingAcceleration = JUMPING_ACCELERATION;
	terminalVelocity = Vec2D(5, 60);//terminalVelocity = Vec2D(10, 20);
	originalColor = color = { 120, 0, 120, 255 };
	alive = true;
	grounded = false;
	jumping = true;
	falling = true;
	gravity = true;
	win = false;
}

void Player::update() {
	updateMotion();
	interactionCheck();
	clearColliders();
	collisionCheck();
	if (!alive) {
		std::cout << "You died. " << std::endl;
		SDL_Delay(1000);
		Game::reset();
	}
	if (win) {
		std::cout << "You win. Congratulations!" << std::endl;
		win = false;
	}
	//std::cout << "V:" << velocity << ", A:" << acceleration << std::endl;
	//std::cout << "Jumping: " << jumping << std::endl;
}

void Player::interactionCheck() {
	jumping = true;
	for (auto collider : colliders) {
		Entity* e = collider.first;
		Vec2D normal = collider.second;
		switch (int(normal.x)) {
			case int(Side::LEFT):

				break;
			case int(Side::RIGHT):

				break;
			default:
				break;
		}
		switch (int(normal.y)) {
			case int(Side::TOP) :
				//std::cout << "Hit ground" << std::endl;
				hitGround();
				switch (e->getId()) {
					case KILL_TILE_ID:
						alive = false;
						break;
					case WIN_TILE_ID:
						win = true;
						break;
					case FALLING_TILE_ID: {
						FallingPlatform* platform = (FallingPlatform*)e;
						if (platform->alive()) {
							platform->subtractLifetime(FPS);
						}
						break;
					}
					default:
						break;
				}
				break;
			case int(Side::BOTTOM):

				break;
			default:
				break;
		}
	}
	if (jumping) {
		acceleration.y = 0;
	}
}

void Player::hitGround() {
	jumping = false;
	Entity::hitGround();
}

void Player::accelerate(Keys key) {
	switch (key) {
		case Keys::LEFT:
			acceleration.x = -movementAcceleration;
			break;
		case Keys::RIGHT:
			acceleration.x = movementAcceleration;
			break;
		case Keys::UP:
			if (!jumping) {
				jumping = true;
				acceleration.y = -jumpingAcceleration;
			}
			break;
		case Keys::DOWN:
			acceleration.y = jumpingAcceleration;
			break;
		default:
			break;
	}
}

void Player::reset() {
	Entity::reset();
	movementAcceleration = MOVEMENT_ACCELERATION;
	jumpingAcceleration = JUMPING_ACCELERATION;
	win = false;
	alive = true;
	jumping = true;
}