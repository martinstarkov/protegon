#pragma once

#include "utils/Vector2.h"

#include "ecs/ECS.h"

namespace engine {

struct Camera {
	Camera() = default;
	Camera(V2_double offset, V2_double scale = { 1.0, 1.0 }) : offset{ offset }, scale{ scale } {}
	V2_double offset{};
	V2_double scale{ 1.0, 1.0 };
	void LimitScale(V2_double max_scale);
	void Center(V2_double point, V2_double size);
};

} // namespace engine