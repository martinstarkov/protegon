#pragma once

#include <vector> // std::vector

#include "math/Vector2.h"
#include "utility/Time.h"

namespace ptgn {

namespace animation {

struct Animation {
	Animation() = default;
	Animation(const V2_int & top_left_pixel,
			  const V2_int & frame_size,
			  const int frame_count,
			  const std::vector<milliseconds>&frame_delays) : top_left_pixel{ top_left_pixel }, frame_size{ frame_size }, frame_count{ frame_count }, frame_delays{ frame_delays } {}
	Animation(const V2_int& top_left_pixel, 
			  const V2_int& frame_size,
			  const int frame_count,
			  const milliseconds frame_delay) : Animation{ top_left_pixel, frame_size, frame_count, std::vector<milliseconds>(frame_count, frame_delay) } {}
	~Animation() = default;
	V2_int top_left_pixel;
	V2_int frame_size;
	int frame_count{ 0 };
	std::vector<milliseconds> frame_delays;
};

} // namespace animation

} // namespace ptgn