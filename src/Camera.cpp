#include "Camera.h"
#include "Player.h"
#include "defines.h"

Camera* Camera::instance = nullptr;

Player* player;

Camera::Camera() {
	player = Player::getInstance();
	pos = Vec2D();
}

Vec2D Camera::centerOnPlayer() {
	bool center = true;
	if (center) {
		return -player->getHitbox().pos - player->getHitbox().size / 2.0f + Vec2D(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	}
	return pos;
}

void Camera::boundaryCheck() {
	if (pos.x < 0) {
		pos.x = 0;
	}
	if (pos.x > WORLD_WIDTH - WINDOW_WIDTH) {
		pos.x = WORLD_WIDTH - WINDOW_WIDTH;
	}
	if (pos.y < 0) {
		pos.y = 0;
	}
	if (pos.y > WORLD_HEIGHT - WINDOW_HEIGHT) {
		pos.y = WORLD_HEIGHT - WINDOW_HEIGHT;
	}
}

void Camera::update() {
	pos = centerOnPlayer();
	//boundaryCheck();
}
