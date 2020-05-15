#include "Player.h"
#include "defines.h"
#include "FallingPlatform.h"
#include "Game.h"

Player* Player::instance = nullptr;

#define MOVEMENT_ACCELERATION 1.0f
#define JUMPING_ACCELERATION 1.0f

void Player::init() {
	hitbox = { Vec2D(700, 1), Vec2D(128, 128) };
	id = PLAYER_ID;
	originalPos = hitbox.pos;
	velocity = {};
	acceleration = {};
	movementAcceleration = MOVEMENT_ACCELERATION;
	jumpingAcceleration = JUMPING_ACCELERATION;
	terminalVelocity = Vec2D(500, 500);//terminalVelocity = Vec2D(10, 20);
	originalColor = color = { 120, 0, 120, 255 };
	alive = true;
	falling = true;
	gravity = true;
	win = false;
}

void Player::update() {
	if (!alive) {
		std::cout << "You died. " << std::endl;
		SDL_Delay(3000);
		Game::reset();
	}
	if (win) {
		std::cout << "You win. Congratulations!" << std::endl;
		win = false;
	}
	Entity::update();
	//std::cout << "V:" << velocity << ", A:" << acceleration << std::endl;
}

void Player::resolveCollision() {
	//jumping = true;
	//grounded = false;
	//Entity* entity = collided(Side::ANY);
	//if (entity) {
	//	switch (entity->getId()) {
	//		case KILL_TILE_ID:
	//			//alive = false;
	//			break;
	//		case WIN_TILE_ID:
	//			//win = true;
	//		default:
	//			break;
	//	}
	//}
	//entity = collided(Side::BOTTOM);
	//if (entity) {
	//	hitGround();
	//	switch (entity->getId()) {
	//		case FALLING_TILE_ID: {
	//			FallingPlatform* platform = (FallingPlatform*)entity;
	//			if (platform->alive()) {
	//				//platform->subtractLifetime(FPS);
	//			}
	//			break;
	//		}
	//		default:
	//			break;
	//	}
	//}
	//if (collided(Side::TOP)) {
	//	velocity.y *= -1 / 2;
	//	acceleration.y *= -1 / 10;
	//}
	//if (collided(Side::RIGHT) || collided(Side::LEFT)) {
	//	velocity.x = 0;
	//}
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
			acceleration.y = -movementAcceleration;
			//if (!jumping) {
			//	counter = 0;
			//	jumping = true;
			//} else {
			//	counter++;
			//}
			break;
		case Keys::DOWN:
			acceleration.y = movementAcceleration;
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