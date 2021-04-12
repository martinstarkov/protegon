#pragma once

#include "math/Vector2.h"

namespace engine {

struct Transform {
	Transform() = default;
	Transform(const V2_double& position) : position{ position } {}
	Transform(const V2_double& position, const double rotation) : position{ position }, rotation{ rotation } {}
	V2_double position;
	double rotation{ 0 };
};

} // namespace engine