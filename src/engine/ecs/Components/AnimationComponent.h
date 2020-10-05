#pragma once

#include "Component.h"
#include <Defines.h>

using AnimationName = std::string;

struct AnimationComponent {
	AnimationName name; // name of current animation
	int sprites; // amount of sprites in current animation
	double animationDelay; // second delay between animation frames
	int frame; // frame of animation
	int cyclesPerFrame;
	int counter;
	AnimationComponent(int sprites = 1, double animation_delay = 0.1, int frame = 0) : sprites{ sprites }, animationDelay{ animation_delay }, frame{ frame }, name{ "" } {
		Init();
	}
	void Init() {
		cyclesPerFrame = static_cast<int>(round(FPS * animationDelay));
		counter = cyclesPerFrame * frame;
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const AnimationComponent& o) {
	j["sprites"] = o.sprites;
	j["animationDelay"] = o.animationDelay;
	j["frame"] = o.frame;
}

inline void from_json(const nlohmann::json& j, AnimationComponent& o) {
	if (j.find("sprites") != j.end()) {
		o.sprites = j.at("sprites").get<int>();
	}
	if (j.find("animationDelay") != j.end()) {
		o.animationDelay = j.at("animationDelay").get<double>();
	}
	if (j.find("frame") != j.end()) {
		o.frame = j.at("frame").get<int>();
	}
	o.Init();
}