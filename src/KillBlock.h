#pragma once
#include "Entity.h"
#include "defines.h"

class KillBlock : public Entity {
public:
	KillBlock(AABB hitbox) : Entity({ hitbox }) {
		id = KILL_TILE_ID;
		originalColor = color = { 255, 0, 0, 255 };
	}
private:

};

