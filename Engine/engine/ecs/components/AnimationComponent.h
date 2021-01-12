#pragma once

#include "Component.h"

#include "utils/Math.h"
#include "core/Engine.h"

struct AnimationComponent {
	std::string current_animation;
	double animation_delay; // Second delay between animation frames.
	int frame; // Frame of animation.
	int cycles_per_frame;
	int counter;
	AnimationComponent(const std::string& starting_animation, double animation_delay = 0.1, int frame = 0) :
		current_animation{ starting_animation },
		animation_delay{ animation_delay }, 
		frame{ frame } {
		Init();
	}
	void Init() {
		cycles_per_frame = static_cast<int>(engine::math::FastRound(static_cast<double>(engine::Engine::FPS()) * animation_delay));
		counter = cycles_per_frame * frame;
	}
};