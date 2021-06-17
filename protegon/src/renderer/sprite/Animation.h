#pragma once

#include <cstdlib> // std::size_t

#include "math/Vector2.h"

namespace ptgn {

struct Animation {
	Animation() = default;
	Animation(const V2_int& top_left_position, 
			  const V2_int& sprite_sheet_size, 
			  std::size_t sprite_count, 
			  std::size_t spacing_between_sprites = 0) :
		position{ top_left_position }, 
		size{ sprite_sheet_size }, 
		count{ sprite_count },
		spacing{ spacing_between_sprites } {}
	V2_int position;
	V2_int size;
	std::size_t count{ 0 };
	std::size_t spacing{ 0 };
};

} // namespace ptgn