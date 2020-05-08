#pragma once
#include "Entity.h"

enum Keys {
	LEFT,
	RIGHT,
	UP,
	DOWN
};
enum Axis {
	VERTICAL,
	HORIZONTAL,
	BOTH
};

class Player : public Entity {
private:
	static Player* instance;
	bool broadPhaseCheck(AABB bpb, Entity* entity);
	void hitGround();
	Vec2D originalPos;
public:
	void resetPosition();
	bool jumping = false;
	bool grounded = false;
	void boundaryCheck();
	void move(Keys key);
	void stop(Axis axis);
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
	bool collisionCheck(Entity* entity);
};

