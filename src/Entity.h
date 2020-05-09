#pragma once
#include "Vec2D.h"
#include "AABB.h"
#include <vector>

enum Axis {
	VERTICAL,
	HORIZONTAL,
	BOTH
};

class Entity {
private:

public:
	Entity(AABB hitbox, Vec2D vel = {}, Vec2D accel = {}, bool hasGravity = false) : hitbox(hitbox), velocity(vel), acceleration(accel), hasGravity(hasGravity) {}
	Entity(Vec2D vel = {}, Vec2D accel = {}, bool hasGravity = false) : velocity(vel), acceleration(accel), hasGravity(hasGravity) {}
	void update();
	void stop(Axis axis);
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
	void updateMotion();
	void collisionCheck();
	void clearColliders();
	std::vector<Vec2D> yCollisions;
	AABB hitbox;
	Vec2D velocity;
	Vec2D acceleration;
	bool hasGravity;
	bool isColliding(Entity* entity);
	bool broadphaseCheck(AABB bpb, Entity* entity);
};

