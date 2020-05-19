#pragma once
#include "Matrix2D.h"
#include "AABB.h"

#define CAMERA_SPEED 5.0f

class Camera {
public:
	static Camera* getInstance() {
		if (!instance) {
			instance = new Camera();
		}
		return instance;
	}
	Camera();
	void update();
	void setPosition(Vec2D newPos) {
		pos = newPos;
	}
	void addPosition(Vec2D delta) {
		pos += delta * CAMERA_SPEED;
	}
	Vec2D getPosition() {
		return pos;
	}
private:
	Vec2D centerOnPlayer();
	void boundaryCheck();
	static Camera* instance;
	Vec2D pos;
};

