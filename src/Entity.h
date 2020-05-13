#pragma once
#include "Vec2D.h"
#include "AABB.h"
#include "defines.h"
#include <vector>

#define DRAG 0.1f
#define GRAVITY 10.0f

enum class Axis {
	VERTICAL,
	HORIZONTAL,
	BOTH
};

enum class Side {
	TOP,
	BOTTOM,
	LEFT,
	RIGHT,
	ANY
};

class Entity {
public:
	Entity(AABB hitbox, Vec2D vel = {}, Vec2D accel = {}, Vec2D termVel = {}, bool falling = false, int id = UNKNOWN_TILE, SDL_Color col = { 0, 0, 255, 255 }) : hitbox(hitbox), oldHitbox(hitbox), velocity(vel), acceleration(accel), originalPos(hitbox.pos), falling(falling), gravity(falling), id(id), color(col), originalColor(col) {
		if (termVel == Vec2D()) {
			terminalVelocity = Vec2D().infinite();
		} else {
			terminalVelocity = termVel;
		}
		grounded = false;
		g = GRAVITY;
		tempHitbox = hitbox;
	}
	Entity(Vec2D vel = {}, Vec2D accel = {}) : Entity({}, vel, accel) {}
	virtual void update();
	void stop(Axis direction);
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
	Vec2D getPosition() {
		return hitbox.pos;
	}
	AABB getHitbox() {
		return hitbox;
	}
	AABB getOldHitbox() {
		return oldHitbox;
	}
	AABB getTempHitbox() {
		return tempHitbox;
	}
	int getId() {
		return id;
	}
	void setId(int newId) {
		id = newId;
	}
	void setGravity(bool fall) {
		gravity = fall;
	}
	SDL_Color getColor() {
		return color;
	}
	void setColor(SDL_Color newColor) {
		color = newColor;
	}
	bool getGravity() {
		return gravity;
	}
	bool isGrounded() {
		return grounded;
	}
	void setGrounded(bool grounding) {
		grounded = grounding;
	}
	virtual void reset();
	virtual void accelerate(Axis direction, float movementAccel);
protected:
	int id;
	std::vector<std::pair<Entity*, Vec2D>> yCollisions;
	std::vector<std::pair<Entity*, Vec2D>> xCollisions;
	AABB hitbox;
	AABB oldHitbox;
	AABB tempHitbox;
	SDL_Color color;
	SDL_Color originalColor;
	Vec2D velocity;
	Vec2D acceleration;
	Vec2D originalPos;
	Vec2D terminalVelocity;
	float g;
	bool gravity;
	bool falling;
	bool grounded;
	void updateMotion();
	void boundaryCheck();
	void terminalMotion();
	void collisionCheck();
	AABB broadphaseBox(AABB a, Vec2D vel);
	int IntersectMovingAABBAABB(AABB a, AABB b, Vec2D va, Vec2D vb, float& tfirst, float& tlast);
	int TestAABBAABB(AABB a, AABB b);
	Vec2D lineLine(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);
	Vec2D lineRect(float x1, float y1, float x2, float y2, float rx, float ry, float rw, float rh);
	void staticCheck(Entity* e);
	virtual void resolveCollision();
	Entity* collided(Side side);
	void clearColliders();
	virtual void hitGround();
private:
};