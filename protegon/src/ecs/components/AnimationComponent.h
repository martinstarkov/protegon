#pragma once

#include "Component.h"

#include "math/Math.h"
#include "core/Engine.h"

// TODO: Fix cycles_per_frame

//namespace engine {
//
//struct AnimationComponent {
//	const char* current_animation;
//	double animation_delay; // Second delay between animation frames.
//	int frame; // Frame of animation.
//	int cycles_per_frame;
//	int counter;
//	AnimationComponent(const char* starting_animation, double animation_delay = 0.1, int frame = 0) :
//		current_animation{ starting_animation },
//		animation_delay{ animation_delay },
//		frame{ frame } {
//		Init();
//	}
//	void Init() {
//		//cycles_per_frame = math::Round(Engine::GetFPS() * animation_delay);
//		//counter = cycles_per_frame * frame;
//	}
//};
//
//} // namespace engine