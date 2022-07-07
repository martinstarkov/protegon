#pragma once

#include <cstdlib> // std::size_t
#include "math/Vector2.h"

namespace ptgn {

namespace animation {

struct Animation {
	Animation() = default;
	Animation(const V2_int& top_left_pixel, const V2_int& frame_size, const std::size_t frame_count, const std::size_t frame = 0) : top_left{ top_left_pixel }, size{ frame_size }, frames{ frame_count }, frame{ frame } {}
	V2_int top_left;
	V2_int size;
	std::size_t frames{ 0 };
	std::size_t frame{ 0 };
};

} // namespace animation

} // namespace ptgn