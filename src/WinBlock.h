#pragma once
#include "Entity.h"
#include "defines.h"

class WinBlock : public Entity {
public:
	WinBlock(AABB hitbox) : Entity({ hitbox }) {
		id = WIN_TILE_ID;
		originalColor = color = { 0, 140, 0, 255 };
	}
private:

};
