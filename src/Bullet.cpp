#include "Bullet.h"
#include "LevelController.h"
#include "Entities.h"

void Bullet::init() {
	originalColor = color = { 0, 0, 0, 255 };
	terminalVelocity = Vec2D(20, 20);
	gravity = true;
	g = 0.0f;
}

void Bullet::update() {
	updateMotion();
	interactionCheck();
	clearColliders();
	collisionCheck();
	subtractLifetime(FPS);
}

void Bullet::updateMotion() {
	//velocity *= (1.0f - DRAG); // drag
	if (gravity) {
		velocity.y += g;
	}
	terminalMotion(velocity);
}

void Bullet::interactionCheck() {
	for (auto collider : colliders) {
		Entity* e = collider.first;
		Vec2D normal = collider.second;
		if (normal) {
			velocity = Vec2D();
			Level* l = LevelController::getCurrentLevel();
			switch (e->getId()) {
				case KILL_TILE_ID:
					lifetime = 0;
					e->setAlive(false);
					break;
				case FALLING_TILE_ID: {
					FallingPlatform* platform = (FallingPlatform*)e;
					platform->reset();
					lifetime = 0;
					break;
				}
				default:
					break;
			}
		}
	}
}