#pragma once

#include "utils/math/Vector2.h"

#include "ecs/ECS.h"

namespace engine {

struct Camera {
	Camera() = default;
	Camera(V2_double offset, V2_double scale) : offset{ offset }, scale{ scale } {}
	Camera(V2_double scale) : scale{ scale } {}
	V2_double offset{};
	V2_double scale{ 1.0, 1.0 };
	void LimitScale(V2_double max_scale);
	void Center(V2_double point, V2_double size);
};

} // namespace engine