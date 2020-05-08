#pragma once
#include "Vec2D.h"
#include "AABB.h"
#include <vector>

class Entity {
private:

public:
	AABB oldHitbox;
	Entity(AABB hitbox, Vec2D vel = {}, Vec2D accel = {}) : hitbox(hitbox), velocity(vel), acceleration(accel) {}
	Entity(Vec2D vel = {}, Vec2D accel = {}) : velocity(vel), acceleration(accel) {}
	void update();
	void setAcceleration(Vec2D newAccel) {
		acceleration = newAccel;
	}
	Vec2D getAcceleration() {
		return acceleration;
	}
	void setVelocity(Vec2D newVel) {
		velocity = newVel;
	}
	Vec2D getVelocity() {
		return velocity;
	}
	void setPosition(Vec2D newPosition) {
		hitbox.pos = newPosition;
	}
	AABB getHitbox() {
		return hitbox;
	}
protected:
	AABB hitbox;
	Vec2D velocity;
	Vec2D acceleration;
	//bool checkCollision(Entity* entity);
	//void resolveCollision(Entity* entity, Vec2D dir);
	//std::vector<Entity*> broadphaseCheck(Entity* entity);
};

