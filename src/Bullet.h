#pragma once
#include "Entity.h"

class Bullet : public Entity {
public:
	Bullet(AABB hitbox, float life = 1.0f) : Entity{ hitbox } {
		originalLifetime = lifetime = int(life * 1000.0f);
		init();
	}
	bool alive() {
		return lifetime > 0;
	}
	void subtractLifetime(int amount) {
		if (lifetime - amount >= 0) {
			lifetime -= amount;
		} else {
			lifetime = 0;
		}
	}
	void update();
private:
	void init();
	void updateMotion();
	void interactionCheck();
private:
	int lifetime;
	int originalLifetime;
};

