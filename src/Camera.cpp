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
	// these work fine
	if (pos.x * scale.x > 0) {
		pos.x = 0;
	}
	if (pos.y * scale.y > 0) {
		pos.y = 0;
	}
	//// these are broken for zooming
	//if (pos.x < (WINDOW_WIDTH - WORLD_WIDTH)) {
	//	pos.x = (WINDOW_WIDTH - WORLD_WIDTH) ;
	//}
	//if (pos.y < (WINDOW_HEIGHT - WORLD_HEIGHT)) {
	//	pos.y = (WINDOW_HEIGHT - WORLD_HEIGHT) ;
	//}
}

void Camera::update() {
	pos = centerOnPlayer();
	boundaryCheck();
}
