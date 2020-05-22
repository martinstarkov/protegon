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
	void interactionCheck();
	void hitGround();
	bool alive;
	bool win;
	float jumpingAcceleration;
	float movementAcceleration;
	int counter = 0;
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
	bool jumping = true;
	void update();
	void accelerate(Keys key);
};

