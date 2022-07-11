#pragma once

#include <cstdlib> // std::size_t
#include <cassert> // assert

#include "math/Vector2.h"
#include "animation/Animation.h"
#include "animation/SpriteMap.h"
#include "utility/Countdown.h"

namespace ptgn {

namespace animation {

struct AnimationState {
	AnimationState() = delete;
	AnimationState(SpriteMap& sprite_map, std::size_t animation_key, std::size_t start_frame = 0, bool start = false) : sprite_map{ sprite_map }, animation_key_{ animation_key }, current_frame_{ start_frame } {
		if (start) {
			Start();
		} else {
			ResetRemaining();
		}
	}
	void IncrementFrame() {
		current_frame_ = (current_frame_ + 1) % GetAnimation().frame_count;
	}
	void ResetRemaining() {
		auto& animation{ GetAnimation() };
		assert(current_frame_ < animation.frame_delays.size());
		countdown_.SetRemaining(animation.frame_delays[current_frame_]);
	}
	const Animation& GetAnimation() const {
		assert(sprite_map.Has(animation_key_) && "Animation not found in sprite map");
		return *sprite_map.Get(animation_key_);
	}
	void Start() {
		ResetRemaining();
		countdown_.Start();
	}
	void Update() {
		if (countdown_.Finished()) {
			IncrementFrame();
			Start();
		}
	}
	V2_int GetCurrentPosition() const {
		auto& animation{ GetAnimation() };
		return { animation.top_left_pixel.x + animation.frame_size.x * current_frame_, animation.top_left_pixel.y };
	}
	SpriteMap& sprite_map;
private:
	std::size_t animation_key_;
	std::size_t current_frame_{ 0 };
	Countdown countdown_;
};

} // namespace animation

} // namespace ptgn