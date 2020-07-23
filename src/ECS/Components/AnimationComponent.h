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
	AnimationComponent(int sprites = 1, double animationDelay = 0.1, int frame = 0) : sprites(sprites), animationDelay(animationDelay), frame(frame) {
		cyclesPerFrame = static_cast<int>(round(FPS * animationDelay));
		counter = cyclesPerFrame * frame;
		name = "";
	}
	friend std::ostream& operator<<(std::ostream& out, const AnimationComponent& obj) {
		out << "{" << std::endl;
		out << "sprites: " << obj.sprites << ";" << std::endl;
		out << "animationDelay: " << obj.animationDelay << ";" << std::endl;
		out << "frame: " << obj.frame << ";" << std::endl;
		out << "cyclesPerFrame: " << obj.cyclesPerFrame << ";" << std::endl;
		out << "counter: " << obj.counter << ";" << std::endl;
		out << "}" << std::endl;
		return out;
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const AnimationComponent& o) {
	j["sprites"] = o.sprites;
	j["animationDelay"] = o.animationDelay;
	j["frame"] = o.frame;
}

inline void from_json(const nlohmann::json& j, AnimationComponent& o) {
	o = AnimationComponent(
		j.at("sprites").get<int>(), 
		j.at("animationDelay").get<float>(),
		j.at("frame").get<int>()
	);
}