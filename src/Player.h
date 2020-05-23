#pragma once
#include "Entity.h"
#include "defines.h"
#include "Bullet.h"

enum class Keys {
	LEFT,
	RIGHT,
	UP,
	DOWN
};

class Player : public Entity {
public:
	Player() : Entity{} {
		init();
	}
	static Player* getInstance() {
		if (!instance) {
			instance = new Player();
		}
		return instance;
	}
	void setMovementAcceleration(float newMA) {
		movementAcceleration = newMA;
	}
	void reset();
	void shoot();
	bool jumping = true;
	void update();
	void accelerate(Keys key);
	std::vector<Bullet*> getProjectiles() {
		return projectiles;
	}
	void projectileLifeCheck();
private:
	void init();
	void interactionCheck();
	void hitGround();
private:
	static Player* instance;
	bool alive;
	bool win;
	float jumpingAcceleration;
	float movementAcceleration;
	int counter = 0;
	std::vector<Bullet*> projectiles;
};

