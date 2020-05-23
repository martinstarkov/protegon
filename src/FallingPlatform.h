#pragma once
#include "Entity.h"
#include "defines.h"

class FallingPlatform : public Entity {
public:
	FallingPlatform(AABB hitbox, float life = 1.0f) : Entity({ hitbox }) {
		id = FALLING_TILE_ID;
		originalColor = color = { 0, 0, 255, 255 };
		falling = fallen = false;
		originalLifetime = lifetime = int(life * 1000.0f);
	}
	// lifetime measured in game ticks (from SDL_GetTicks())
	int getLifetime() {
		return lifetime;
	}
	int getOriginalLifetime() {
		return originalLifetime;
	}
	void setLifetime(int newLife) {
		lifetime = newLife;
	}
	void subtractLifetime(int amount) { 
		if (lifetime - amount >= 0) {
			lifetime -= amount;
		} else {
			lifetime = 0;
		}
	}
	void decreaseLifetime() {
		if (alive()) {
			lifetime--;
		}
	}
	bool alive() {
		return lifetime > 0;
	}
	void reset();
	void update();
private:
	bool fallen;
	int lifetime;
	int originalLifetime;
};

