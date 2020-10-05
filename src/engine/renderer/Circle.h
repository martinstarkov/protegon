#pragma once

#include <engine/renderer/Shape.h>
#include <engine/utils/Vector2.h>

struct Circle : Shape<Circle> {
	Circle() : position{}, radius{ 0 } {}
	Circle(V2_double position, double radius) : position{ position }, radius{ radius } {}
	V2_double position;
	double radius;
};