#pragma once

#include "Component.h"

struct AnimationComponent : public Component<AnimationComponent> {
	unsigned int _state;
	unsigned int _framesBetween;
	unsigned int _counter;
	unsigned int _sprites;
	// Animation delay in seconds
	AnimationComponent(float animationDelay = 1.0f, unsigned int state = 0) : _state(state), _framesBetween(static_cast<unsigned int>(FPS * animationDelay)),  _counter(_framesBetween * _state) {
		_sprites = 8;
	}
};