#pragma once
#include "Entity.h"

class Player : public Entity {
private:
	static Player* instance;
public:
	bool jumping = false;
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
	void collisionCheck(Entity* entity);
};

