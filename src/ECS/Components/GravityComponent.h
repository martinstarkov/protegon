#pragma once

#include "Component.h"

#include "../../Vec2D.h"

#define DEFAULT_GRAVITY 0.001f

struct GravityComponent : public Component<GravityComponent> {
	float _g;
	Vec2D _direction;
	GravityComponent(float g = DEFAULT_GRAVITY, Vec2D direction = Vec2D(0, 1)) : _g(g), _direction(direction) {}
};