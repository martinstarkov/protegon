#pragma once

#include "Vec2D.h"

struct Ray2D {
	Vec2D origin;
	Vec2D direction;
	Ray2D() = default;
	Ray2D(Vec2D origin, Vec2D direction) : origin(origin), direction(direction) {}
};