#pragma once

#include "math/Vector2.h"

#include "ecs/ECS.h"

namespace engine {

struct Camera {
	Camera() = default;
	Camera(const V2_double& offset, const V2_double& scale) : offset{ offset }, scale{ scale } {}
	Camera(const V2_double& scale) : scale{ scale } {}
	V2_double offset;
	V2_double scale{ 1.0, 1.0 };
	void ClampScale(const V2_double& max_scale);
	void Center(const V2_double& point, const V2_double& size);
};

} // namespace engine