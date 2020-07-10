#pragma once

#include "Component.h"

struct AnimationComponent : public Component<AnimationComponent> {
	int _sprites;
	float _animationDelay;
	int _state;
	int _cyclesPerFrame;
	int _counter;
	// Animation delay in seconds
	AnimationComponent(int sprites = 1, float animationDelay = 0.1f, int state = 0) : _sprites(sprites), _animationDelay(animationDelay), _state(state) {
		_cyclesPerFrame = static_cast<int>(std::roundf(FPS * _animationDelay));
		_counter = _cyclesPerFrame * _state;
	}
	friend std::ostream& operator<<(std::ostream& out, const AnimationComponent& obj) {
		out << "{" << std::endl;
		out << "_sprites: " << obj._sprites << ";" << std::endl;
		out << "_animationDelay: " << obj._animationDelay << ";" << std::endl;
		out << "_state: " << obj._state << ";" << std::endl;
		out << "_cyclesPerFrame: " << obj._cyclesPerFrame << ";" << std::endl;
		out << "_counter: " << obj._counter << ";" << std::endl;
		out << "}" << std::endl;
		return out;
	}
};

// json serialization
inline void to_json(nlohmann::json& j, const AnimationComponent& o) {
	j["_sprites"] = o._sprites;
	j["_animationDelay"] = o._animationDelay;
	j["_state"] = o._state;
}

inline void from_json(const nlohmann::json& j, AnimationComponent& o) {
	o = AnimationComponent(
		j.at("_sprites").get<int>(), 
		j.at("_animationDelay").get<float>(),
		j.at("_state").get<int>()
	);
}