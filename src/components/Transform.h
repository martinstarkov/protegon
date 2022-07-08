#pragma once

#include "math/Vector2.h"

namespace ptgn {

struct Transform {
	Transform() = default;
	~Transform() = default;
	Transform(const V2_double& position) : position{ position } {}
	Transform(const V2_double& position, const double rotation) : 
		position{ position }, rotation{ rotation } {}
	
	V2_double position;
	double rotation{ 0 };
};

struct OriginalTransform : public Transform {
	using Transform::Transform;
	OriginalTransform(const Transform& transform) : Transform{ transform } {}
};

} // namespace ptgn