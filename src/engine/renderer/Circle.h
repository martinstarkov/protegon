#pragma once

#include <Vec2D.h>

#include <engine/renderer/Shape.h>

struct Circle : Shape<Circle> {
	Circle() : position(), radius(0.0) {}
	Circle(Vec2D position, Vec2D radius) : position(position), radius(radius) {}
	Vec2D position;
	double radius;
};