#pragma once

#include <cstdlib> // std::size_t
#include <vector> // std::vector
#include <cassert> // assert

#include "math/Vector2.h"
#include "utility/Countdown.h"
#include "managers/ResourceManager.h"

namespace ptgn {

namespace animation {

struct Animation {
	Animation() = default;
	Animation(const V2_int& top_left_pixel, 
			  const V2_int& frame_size,
			  const std::size_t frame_count,
			  const milliseconds frame_delay) : top_left_pixel{ top_left_pixel }, frame_size{ frame_size }, frame_count{ frame_count }, frame_delays{ frame_count, frame_delay } {}
	Animation(const V2_int& top_left_pixel,
			  const V2_int& frame_size,
			  const std::size_t frame_count,
			  const std::vector<milliseconds>& frame_delays) : top_left_pixel{ top_left_pixel }, frame_size{ frame_size }, frame_count{ frame_count }, frame_delays{ frame_delays } {}
	~Animation() = default;
	V2_int top_left_pixel;
	V2_int frame_size;
	std::size_t frame_count{ 0 };
	std::vector<milliseconds> frame_delays;
};

struct AnimationState {
	AnimationState() = default;
	AnimationState(Animation* animation, std::size_t start_frame = 0, bool start = false) : animation{ animation }, current_frame{ start_frame } {
		assert(Exists() && "Cannot create animation state from nonexistent animation");
		if (start) {
			Start();
		} else {
			ResetRemaining();
		}
	}
	bool Exists() const {
		return animation != nullptr;
	}
	void IncrementFrame() {
		assert(Exists() && "Cannot increment frame of nonexistent animation");
		current_frame = (current_frame + 1) % animation->frame_count;
	}
	void ResetRemaining() {
		assert(Exists() && "Cannot reset countdown time of nonexistent animation");
		assert(current_frame < animation->frame_delays.size());
		countdown.SetRemaining(animation->frame_delays[current_frame]);
	}
	void Start() {
		assert(Exists() && "Cannot start nonexistent animation");
		ResetRemaining();
		countdown.Start();
	}
	void Update() {
		if (countdown.Finished()) {
			IncrementFrame();
			Start();
		}
	}
	V2_int GetCurrentPosition() const {
		assert(Exists() && "Cannot get current position of nonexistent animation");
		return { animation->top_left_pixel.x + animation->frame_size.x * current_frame, animation->top_left_pixel.y };
	}
	Animation* animation{ nullptr };
	std::size_t current_frame{ 0 };
	Countdown countdown;
};

class AnimationMap : public managers::ResourceManager<AnimationState> {};

} // namespace animation

} // namespace ptgn