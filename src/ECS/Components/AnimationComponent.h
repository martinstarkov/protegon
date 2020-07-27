#pragma once

#include "Component.h"

struct AnimationComponent : public Component<AnimationComponent> {
	AnimationName name;
	int sprites;
	double animationDelay;
	int frame;
	int cyclesPerFrame;
	int counter;
	// Animation delay in seconds
	AnimationComponent(int sprites = 1, double animationDelay = 0.1, int frame = 0) : sprites(sprites), animationDelay(animationDelay), frame(frame), name("") {
		init();
	}
	void init() {
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
	o.init();
}