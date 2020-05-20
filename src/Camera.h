#pragma once
#include "Matrix2D.h"
#include "AABB.h"

#define CAMERA_SPEED 5.0f
#define CAMERA_ZOOM_SPEED 0.1f
#define ZOOM_BOUNDARY 0.5f

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
	Vec2D worldToScreen(Vec2D worldPos) {
		return (worldPos - pos) * scale;
	}
	Vec2D screenToWorld(Vec2D screenPos) {
		return screenPos / scale + pos;
	}
	Vec2D getScale() {
		return scale;
	}
	void resetScale() {
		scale = Vec2D(1.0f, 1.0f);
	}
	void multiplyScale(float factor) {
		scale *= factor;
		zoomLimit();
	}
private:
	void zoomLimit();
	Vec2D centerOnPlayer();
	void boundaryCheck();
	static Camera* instance;
	Vec2D pos;
	Vec2D scale;
	Vec2D startPan;
};

