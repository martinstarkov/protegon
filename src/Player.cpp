#include "Player.h"
#include "defines.h"
#include "FallingPlatform.h"
#include "Game.h"
#include "LevelController.h"
#include <algorithm>

Player* Player::instance = nullptr;

#define MOVEMENT_ACCELERATION 0.8f
#define JUMPING_ACCELERATION 15.0f

void Player::init() {
	direction = Direction::RIGHT;
	hitbox = { Vec2D(), Vec2D(32, 32) };
	id = PLAYER_ID;
	originalPos = hitbox.pos;
	velocity = {};
	acceleration = {};
	movementAcceleration = MOVEMENT_ACCELERATION;
	jumpingAcceleration = JUMPING_ACCELERATION;
	terminalVelocity = Vec2D(5, 20);//terminalVelocity = Vec2D(10, 20);
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
		if (LevelController::changeCurrentLevel(-1)) {
			std::cout << "You died. Back to " << LevelController::getCurrentLevel()->getName() << std::endl;
		} else {
			std::cout << "You died. Can't even pass the tutorial?" << std::endl;
		}
		if (Game::attempts == 7) {
			std::cout << "GETTING FRUSTRATED YET?" << std::endl;
		}
		if (Game::attempts == 14) {
			std::cout << "I WONDER IF YOU'LL EVER BEAT THIS GAME..." << std::endl;
		}
		Game::attempts++;
		SDL_Delay(1000);
		Game::getInstance()->reset();
	}
	if (win) {
		if (LevelController::changeCurrentLevel(1)) {
			if (LevelController::getCurrentLevel()->getId() == 4) {
				std::cout << "Congratulations! You beat the game in " << Game::attempts << " attempt(s)!" << std::endl;
				if (Game::attempts > 2) {
					std::cout << "I beat it in 2 attempts ;)" << std::endl;
					std::cout << "I challenge you to beat my record :P" << std::endl;
				} else if (Game::attempts == 2) {
					std::cout << "You tied my record :) I challenge you to beat it next time!" << std::endl;
				} else {
					std::cout << "You beat my record! :O" << std::endl;
					SDL_Delay(2000);
					std::cout << "Just kidding... of course I beat the game in 1 attempt.. who couldn't??? It's easy :P" << std::endl;
				}
			} else {
				std::cout << "Advancing to " << LevelController::getCurrentLevel()->getName() << std::endl;
			}
		}
		SDL_Delay(1000);
		Game::getInstance()->reset();
	}
	terminalMotion(velocity);
	//std::cout << "V:" << velocity << ", A:" << acceleration << std::endl;
	//std::cout << "Jumping: " << jumping << std::endl;
}


void Player::projectileLifeCheck() {
	std::vector<Bullet*> aliveProjectiles;
	for (auto it = projectiles.begin(); it != projectiles.end(); it++) {
		if ((*it)->alive()) {
			aliveProjectiles.push_back(*it);
		}
	}
	projectiles = aliveProjectiles;
}

#define BULLET_LIFE 4.0f // seconds
#define BULLET_SPEED 20.0f
#define BULLET_SIZE Vec2D(10, 10)
#define MAX_BULLET_COUNT 10

void Player::shoot() {
	static int bullets = 1;
	if (projectiles.size() < MAX_BULLET_COUNT) {
		AABB block;
		Vec2D vel = Vec2D();
		block.size = BULLET_SIZE;
		switch (direction) {
			case Direction::LEFT:
				block.pos = Vec2D(hitbox.min().x, hitbox.min().y + hitbox.size.y / 2 - block.size.y / 2);
				vel = Vec2D(-BULLET_SPEED, 0.0f);
				break;
			case Direction::RIGHT:
				block.pos = Vec2D(hitbox.max().x, hitbox.min().y + hitbox.size.y / 2 - block.size.y / 2);
				vel = Vec2D(BULLET_SPEED, 0.0f);
				break;
			default:
				break;
		}
		vel.x += velocity.x;
		vel.y += velocity.y / 30.0f;
		Bullet* bullet = new Bullet(block, BULLET_LIFE);
		bullet->setVelocity(vel);
		bullet->setId(bullets);
		bullets++;
		projectiles.push_back(bullet);
	}
}

void Player::interactionCheck() {
	jumping = true;
	for (auto collider : colliders) {
		Entity* e = collider.first;
		Vec2D normal = collider.second;
		switch (int(normal.x)) {
			case int(Side::LEFT):
				acceleration.x = 0;
				break;
			case int(Side::RIGHT):
				acceleration.x = 0;
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
				case int(Side::BOTTOM) :
					acceleration.y = 0;
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
			direction = Direction::LEFT;
			acceleration.x = -movementAcceleration;
			break;
		case Keys::RIGHT:
			direction = Direction::RIGHT;
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
	projectiles.clear();
	movementAcceleration = MOVEMENT_ACCELERATION;
	jumpingAcceleration = JUMPING_ACCELERATION;
	win = false;
	alive = true;
	jumping = true;
	hitbox.pos = LevelController::getCurrentLevel()->getSpawn();
}