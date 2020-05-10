#pragma once
#include "Entity.h"
#include "defines.h"

enum class Keys {
	LEFT,
	RIGHT,
	UP,
	DOWN
};

class Player : public Entity {
private:
	static Player* instance;
	void init();
	void resolveCollision();
	void hitGround();
	bool alive;
	void reset();
	bool win;
	float jumpingAcceleration = 3.0f;
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
	bool jumping = false;
	void update();
	void accelerate(Keys key);
};

