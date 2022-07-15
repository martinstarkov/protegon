#pragma once

#include <cstdlib> // std::size_t

#include "math/Vector2.h"

namespace ptgn {

namespace animation {

struct Animation {
	Animation() = default;
	Animation(const V2_int& top_left_pixel, 
			  const V2_int& frame_size,
			  const std::size_t frame_count, 
			  const std::size_t current_frame = 0) : top_left_pixel{ top_left_pixel }, frame_size{ frame_size }, frame_count{ frame_count }, current_frame{ current_frame } {}
	V2_int top_left_pixel;
	V2_int frame_size;
	std::size_t frame_count{ 0 };
	std::size_t current_frame{ 0 };
};

} // namespace animation

} // namespace ptgn