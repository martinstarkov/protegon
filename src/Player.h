#pragma once
#include "Entity.h"

enum Keys {
	LEFT,
	RIGHT,
	UP,
	DOWN
};

class Player : public Entity {
private:
	static Player* instance;
	Vec2D originalPos;
	void hitGround();
	void groundCheck();
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
	bool grounded = false;
	void init();
	void update();
	void resetPosition();
	void boundaryCheck();
	void move(Keys key);
};

