#pragma once

#include "Component.h"

#include "utils/Defines.h"
#include "utils/Math.h"

using AnimationName = std::string;

struct AnimationComponent {
	AnimationName name; // name of current animation
	int sprites; // amount of sprites in current animation
	double animation_delay; // second delay between animation frames
	int frame; // frame of animation
	int cycles_per_frame;
	int counter;
	AnimationComponent(int sprites = 1, double animation_delay = 0.1, int frame = 0) : sprites{ sprites }, animation_delay{ animation_delay }, frame{ frame }, name{ "" } {
		Init();
	}
	void Init() {
		cycles_per_frame = engine::math::RoundCast<int>(FPS * animation_delay);
		counter = cycles_per_frame * frame;
	}
};