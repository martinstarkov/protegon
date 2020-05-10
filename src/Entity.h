#pragma once
#include "Vec2D.h"
#include "AABB.h"
#include "defines.h"
#include <vector>

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
	Entity(AABB hitbox, Vec2D vel = {}, Vec2D accel = {}, Vec2D termVel = {}, bool falling = false, int id = UNKNOWN_TILE, SDL_Color col = { 0, 0, 255, 255 }) : hitbox(hitbox), velocity(vel), acceleration(accel), originalPos(hitbox.pos), falling(falling), gravity(falling), id(id), color(col), originalColor(col) {
		if (termVel == Vec2D()) {
			terminalVelocity = Vec2D().infinite();
		} else {
			terminalVelocity = termVel;
		}
		grounded = false;
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
	int getId() {
		return id;
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
	SDL_Color color;
	SDL_Color originalColor;
	Vec2D velocity;
	Vec2D acceleration;
	Vec2D originalPos;
	Vec2D terminalVelocity;
	float g = 0.2f;
	bool gravity;
	bool falling;
	bool grounded;
	void updateMotion();
	void boundaryCheck();
	void terminalMotion();
	void collisionCheck();
	virtual void resolveCollision();
	Entity* collided(Side side);
	void clearColliders();
	virtual void hitGround();
private:
};