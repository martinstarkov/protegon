#pragma once
#include "Vec2D.h"

class Entity {
private:
	int id = -1;
	Vec2D size, pos, vel, accel;
public:
	Entity(Vec2D size = {}, Vec2D pos = {}, Vec2D vel = {}, Vec2D accel = {}, int id = -1) {}
	Vec2D getSize() {
		return size;
	}
	void setSize(Vec2D newSize) {
		size = newSize;
	}
	Vec2D getPosition() {
		return pos;
	}
	void setPosition(Vec2D newPos) {
		pos = newPos;
	}
	Vec2D getVelocity() {
		return vel;
	}
	void setVelocity(Vec2D newVel) {
		vel = newVel;
	}
	Vec2D getAcceleration() {
		return accel;
	}
	void setAcceleration(Vec2D newAccel) {
		 accel = newAccel;
	}
	void update();
};

