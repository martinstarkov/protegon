#pragma once
#include "Vec2D.h"
#include "AABB.h"
#include "defines.h"
#include <vector>

#define DRAG 0.1f
#define GRAVITY 2.0f

enum class Axis {
	HORIZONTAL = 0,
	VERTICAL = 1,
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
	struct Collider {
		Entity* e;
		Vec2D normal, collisionTime;
		Collider(Entity* e, Vec2D normal, Vec2D collisionTime) : e(e), normal(normal), collisionTime(collisionTime) {}
	};
	float sweepAABB(AABB b1, AABB b2, Vec2D v1, Vec2D v2, float& xEntryTime, float& yEntryTime, float& xExitTime, float& yExitTime, float& exit);

	void updateMotion();
	void boundaryCheck();
	void terminalMotion();
	void collisionCheck();
	bool axisOverlapAABB(AABB a, AABB b, Axis axis);
	AABB broadphaseBox(AABB a, Vec2D vel);
	bool sweepAABBvsAABB(AABB a, AABB b, Vec2D va, Vec2D vb, float& tfirst, float& tlast);
	bool overlapAABBvsAABB(AABB a, AABB b);
	bool equalOverlapAABBvsAABB(AABB a, AABB b);
	Entity* collided(Side side);
	void clearColliders();
	virtual void hitGround();
private:

};