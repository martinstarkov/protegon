#pragma once

#include "Component.h"

struct AnimationComponent : public Component<AnimationComponent> {
	float _animationDelay;
	int _state;
	int _sprites;
	int _cyclesPerFrame;
	int _counter;
	// Animation delay in seconds
	AnimationComponent(float animationDelay = 1.0f, int state = 0, int sprites = 1, int counter = 0) : _animationDelay(animationDelay), _state(state), _sprites(sprites), _counter(counter) {
		_cyclesPerFrame = static_cast<int>(std::roundf(FPS * _animationDelay));
		if (!counter) {
			_counter = _cyclesPerFrame * _state;
		}
	}
	friend std::ostream& operator<<(std::ostream& out, const AnimationComponent& obj) {
		out << "{" << std::endl;
		out << "_animationDelay: " << obj._animationDelay << ";" << std::endl;
		out << "_state: " << obj._state << ";" << std::endl;
		out << "_sprites: " << obj._sprites << ";" << std::endl;
		out << "_cyclesPerFrame: " << obj._cyclesPerFrame << ";" << std::endl;
		out << "_counter: " << obj._counter << ";" << std::endl;
		out << "}" << std::endl;
		return out;
	}
	virtual std::ostream& serialize(std::ostream& out) override {
		out << _animationDelay << std::endl;
		out << _state << std::endl;
		out << _sprites << std::endl;
		out << _counter << std::endl;
		return out;
	}
	static AnimationComponent deserialize(std::istream& in) {
		float animationDelay;
		int state;
		int sprites;
		int counter;
		in >> animationDelay >> state >> sprites >> counter;
		return AnimationComponent(animationDelay, state, sprites, counter);
	}
};