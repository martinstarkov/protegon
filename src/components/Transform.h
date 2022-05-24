#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace component {

struct Transform {
	Transform() = default;
	~Transform() = default;
	Transform(const Transform& copy) = default;
	Transform(Transform&& move) = default;
	Transform& operator=(const Transform& copy) = default;
	Transform& operator=(Transform&& move) = default;

	Transform(const V2_double& position) : position{ position } {}
	Transform(const V2_double& position, const double rotation) : 
		position{ position }, rotation{ rotation } {}
	
	V2_double position;
	double rotation{ 0 };
};

} // namespace component

} // namespace ptgn