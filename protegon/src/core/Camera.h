#pragma once

#include "math/Vector2.h"

namespace engine {

struct Camera {
	Camera() = default;
	Camera(const V2_double& scale,
		   V2_double zoom_speed = { 0.1, 0.1 },
		   V2_double scale_bound = { 5.0, 5.0 });

	void ClampToBound();
	void CenterOn(const V2_double& point, const V2_double& size);

	V2_double position;
	V2_double scale{ 1.0, 1.0 };
	V2_double zoom_speed{ 0.1, 0.1 };
	V2_double scale_bound{ 5.0, 5.0 };
};

} // namespace engine