#pragma once

#include "Component.h"

#include <engine/utils/Defines.h>
#include <engine/utils/Math.h>

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

// json serialization
inline void to_json(nlohmann::json& j, const AnimationComponent& o) {
	j["sprites"] = o.sprites;
	j["animation_delay"] = o.animation_delay;
	j["frame"] = o.frame;
}

inline void from_json(const nlohmann::json& j, AnimationComponent& o) {
	if (j.find("sprites") != j.end()) {
		o.sprites = j.at("sprites").get<int>();
	}
	if (j.find("animation_delay") != j.end()) {
		o.animation_delay = j.at("animation_delay").get<double>();
	}
	if (j.find("frame") != j.end()) {
		o.frame = j.at("frame").get<int>();
	}
	o.Init();
}