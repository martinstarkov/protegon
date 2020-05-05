#pragma once
#include "Vec2D.h"
#include <vector>

class Entity {
private:
	int id = -1;
	Vec2D size, pos, vel, accel;
	std::vector<Vec2D> p;
public:
	Entity(Vec2D size = {}, Vec2D pos = {}, Vec2D vel = {}, Vec2D accel = {}, int id = -1) : size(size), pos(pos), vel(vel), accel(accel), id(id) {
		update();
	}
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
	std::vector<Vec2D>* getPoints() {
		return &p;
	}
	void update();
	bool checkCollision(Entity* entity);
	void resolveCollision(Entity* entity, Vec2D dir);
	float sweptAABB(Entity* entity, Vec2D& normal);
};

