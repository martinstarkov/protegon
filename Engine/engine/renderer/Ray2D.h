#pragma once

#include "utils/Vector2.h"

struct Ray2D {
	V2_double origin;
	V2_double direction;
	Ray2D() = default;
	Ray2D(V2_double origin, V2_double direction) : origin{ origin }, direction{ direction } {}
};