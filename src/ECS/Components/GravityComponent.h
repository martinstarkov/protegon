#pragma once
#include "Component.h"

#define DEFAULT_GRAVITY 0.001f

struct GravityComponent : public Component {
	static ComponentID ID;
	float _g;
	Vec2D _direction;
	GravityComponent(float g = DEFAULT_GRAVITY, Vec2D direction = Vec2D(0, 1)) : _g(g), _direction(direction) {
		ID = createComponentID<GravityComponent>();
	}
};

ComponentID GravityComponent::ID = 0;