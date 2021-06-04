#pragma once

#include "math/Vector2.h"

namespace ptgn {

struct Camera {
	Camera() = default;
	Camera(const V2_double& scale,
		   V2_double zoom_speed = { 0.1, 0.1 },
		   V2_double min_scale = { 0.1, 0.1 },
		   V2_double max_scale = { 5.0, 5.0 });

	void ClampToBound();
	void CenterOn(const V2_double& point, V2_double size = {});

	void ZoomIn();
	void ZoomOut();

	V2_double position;
	V2_double scale{ 1.0, 1.0 };
	V2_double zoom_speed{ 0.1, 0.1 };
	V2_double min_scale = { 0.1, 0.1 };
	V2_double max_scale = { 5.0, 5.0 };
};

} // namespace ptgn