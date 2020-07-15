#pragma once

#include "Component.h"

#include "../../Vec2D.h"

#define DEFAULT_GRAVITY 0.001

struct GravityComponent : public Component<GravityComponent> {
	double g;
	Vec2D direction;
	GravityComponent(double g = DEFAULT_GRAVITY, Vec2D direction = Vec2D(0, 1)) : g(g), direction(direction) {}
};