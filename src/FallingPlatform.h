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
	int getLifetime() { // in game ticks (from SDL_GetTicks())
		return lifetime;
	}
	void setLifetime(int newLife) { // in game ticks (from SDL_GetTicks())
		lifetime = newLife;
	}
	void subtractLifetime(int amount) { // in game ticks (from SDL_GetTicks())
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

