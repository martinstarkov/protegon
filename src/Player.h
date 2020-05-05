#pragma once
#include "Entity.h"

class Player : public Entity {
private:
	static Player* instance;
	Vec2D maxVel;
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
	void init();
	void update();
};

