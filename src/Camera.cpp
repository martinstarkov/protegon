#include "Camera.h"
#include "Player.h"
#include "defines.h"
#include "InputHandler.h"

Camera* Camera::instance = nullptr;

Player* player;

Camera::Camera() {
	player = Player::getInstance();
	pos = Vec2D(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	scale = Vec2D(1.0f, 1.0f);
	startPan = player->getHitbox().pos;
}

void Camera::zoomLimit() {
	if (scale.x > 1.0f + ZOOM_BOUNDARY) {
		scale.x = 1.0f + ZOOM_BOUNDARY;
	}
	if (scale.y > 1.0f + ZOOM_BOUNDARY) {
		scale.y = 1.0f + ZOOM_BOUNDARY;
	}
	if (scale.x < 1.0f - ZOOM_BOUNDARY) {
		scale.x = 1.0f - ZOOM_BOUNDARY;
	}
	if (scale.y < 1.0f - ZOOM_BOUNDARY) {
		scale.y = 1.0f - ZOOM_BOUNDARY;
	}
}

Vec2D Camera::centerOnPlayer() {
	return -player->getHitbox().pos - player->getHitbox().size / 2.0f + Vec2D(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2) / scale;
}

void Camera::boundaryCheck() {
	//if (offset.x < 0) {
	//	offset.x = 0;
	//}
	//if (offset.x > WORLD_WIDTH - WINDOW_WIDTH) {
	//	offset.x = WORLD_WIDTH - WINDOW_WIDTH;
	//}
	//if (offset.y < 0) {
	//	offset.y = 0;
	//}
	//if (offset.y > WORLD_HEIGHT - WINDOW_HEIGHT) {
	//	offset.y = WORLD_HEIGHT - WINDOW_HEIGHT;
	//}
}

void Camera::update() {
	pos = centerOnPlayer();
	//pos += Vec2D(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//boundaryCheck();
}
