#pragma once

#include "math/Vector2.h"

namespace engine {

struct Animation {
	Animation() = delete;
	Animation(const V2_int& top_left, const V2_int& sprite_size, const V2_int& hitbox_offset, int sprite_count, int spacing_between_sprites = 0) :
		position{ top_left }, 
		sprite_size{ sprite_size }, 
		sprite_count{ sprite_count },
		hitbox_offset{ hitbox_offset - top_left },
		spacing{ spacing_between_sprites } {}
	V2_int position;
	V2_int sprite_size;
	V2_int hitbox_offset;
	int sprite_count;
	int spacing;
};

} // namespace engine