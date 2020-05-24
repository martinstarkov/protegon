//#pragma once
//#include "Vec2D.h"
//#include "AABB.h"
//#include "defines.h"
//#include "common.h"
//#include <vector>
//
//#define DRAG 0.1f
//#define GRAVITY 1.0f
//
//class Entity {
//public:
//	Entity(AABB hitbox, Vec2D vel = {}, Vec2D accel = {}, Vec2D termVel = {}, bool falling = false, int id = UNKNOWN_TILE_ID, SDL_Color col = { 0, 0, 255, 255 }) : hitbox(hitbox), velocity(vel), acceleration(accel), originalPos(hitbox.pos), falling(falling), gravity(falling), id(id), color(col), originalColor(col), g(GRAVITY), grounded(false), direction(Direction::RIGHT), alive(true) {
//		if (termVel == Vec2D()) {
//			terminalVelocity = Vec2D().infinite();
//		} else {
//			terminalVelocity = termVel;
//		}
//	}
//	Entity(Vec2D vel = {}, Vec2D accel = {}) : Entity({}, vel, accel) {}
//	void stop(Axis direction);
//	void setAcceleration(Vec2D newAccel) {
//		acceleration = newAccel;
//	}
//	Vec2D getAcceleration() {
//		return acceleration;
//	}
//	void setVelocity(Vec2D newVel) {
//		velocity = newVel;
//	}
//	Vec2D getVelocity() {
//		return velocity;
//	}
//	void setPosition(Vec2D newPosition) {
//		hitbox.pos = newPosition;
//	}
//	Vec2D getPosition() {
//		return hitbox.pos;
//	}
//	AABB getHitbox() {
//		return hitbox;
//	}
//	void setId(int newId) {
//		id = newId;
//	}
//	int getId() {
//		return id;
//	}
//	void setGravity(bool fall) {
//		gravity = fall;
//	}
//	bool getGravity() {
//		return gravity;
//	}
//	void setColor(SDL_Color newColor) {
//		color = newColor;
//	}
//	SDL_Color getColor() {
//		return color;
//	}
//	void setGrounded(bool grounding) {
//		grounded = grounding;
//	}
//	bool isGrounded() {
//		return grounded;
//	}
//	Vec2D getTilePosition() {
//		return tilePos;
//	}
//	void setTilePosition(Vec2D newTilePos) {
//		tilePos = newTilePos;
//	}
//	Direction getDirection() {
//		return direction;
//	}
//	void setDirection(Direction newDirection) {
//		direction = newDirection;
//	}
//	bool getAlive() {
//		return alive;
//	}
//	void setAlive(bool newState) {
//		alive = newState;
//	}
//	virtual void update();
//	virtual void reset();
//	virtual void accelerate(Axis direction, float movementAccel);
//protected:
//	float sweepAABB(AABB b1, AABB b2, Vec2D v1, Vec2D v2, float& xEntryTime, float& yEntryTime, float& xExitTime, float& yExitTime, float& exit);
//	Vec2D findCollisionNormal(std::vector<Vec2D> normals);
//	bool axisOverlapAABB(AABB a, AABB b, Axis axis);
//	AABB maximumBroadphaseBox(AABB a, Vec2D terminalVelocity);
//	AABB broadphaseBox(AABB a, Vec2D vel);
//	bool sweepAABBvsAABB(AABB a, AABB b, Vec2D va, Vec2D vb, float& tfirst, float& tlast, float& xfirst, float& yfirst);
//	bool overlapAABBvsAABB(AABB a, AABB b);
//	bool equalOverlapAABBvsAABB(AABB a, AABB b);
//	Entity* collided(Side side);
//
//	virtual void updateMotion();
//	virtual void boundaryCheck(AABB& hb, Vec2D& vel);
//	virtual void terminalMotion(Vec2D& vel);
//	virtual void collisionCheck();
//	virtual void clearColliders();
//	virtual void hitGround();
//protected:
//	int id;
//	float g;
//	bool gravity;
//	bool falling;
//	bool grounded;
//	bool alive;
//	AABB hitbox;
//	Vec2D velocity;
//	Vec2D acceleration;
//	Vec2D originalPos;
//	Vec2D terminalVelocity;
//	Vec2D tilePos;
//	SDL_Color color;
//	SDL_Color originalColor;
//	Direction direction;
//	std::vector<std::pair<Entity*, Vec2D>> colliders;
//private:
//
//};