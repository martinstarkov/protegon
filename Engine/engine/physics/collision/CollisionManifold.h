#pragma once

#include <ostream> // std::ostream

#include "utils/Vector2.h"

struct CollisionManifold {
	V2_double point;
	V2_double normal;
	double time = 0.0;
	double depth = 0.0;
	friend std::ostream& operator<<(std::ostream& os, const CollisionManifold& obj) {
		os << "Point: " << obj.point << ", Normal: " << obj.normal << ", Time: " << obj.time << ", Depth: " << obj.depth;
		return os;
	}
};