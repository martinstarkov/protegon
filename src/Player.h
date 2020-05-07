#pragma once
#include "Entity.h"

class Player : public Entity {
private:
	static Player* instance;
	bool broadPhaseCheck(AABB bpb, Entity* entity);
	void hitGround();
public:
	bool jumping = false;
	bool grounded = false;
	void boundaryCheck();
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

