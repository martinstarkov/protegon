#pragma once

#include "math/Vector2.h"

namespace ptgn {

struct Camera {
	Camera() = default;
	~Camera() = default;

	// Zoom camera in by the set zoom speed.
	void ZoomIn();
	void ZoomIn(const V2_float& amount);

	// Zoom camera out by the set zoom speed.
	void ZoomOut();
	void ZoomOut(const V2_float& amount);
	// Center camera on a point with a size.
	void CenterOn(const V2_float& point, const V2_float& size = {});

	V2_float RelativePosition(const V2_float& object_position);
	V2_float RelativeSize(const V2_float& object_size);
private:
	// Clamp camera zoom to minimum and maximum.
	void ClampZoom();

	V2_float position;
	V2_float scale{ 1, 1 };
	V2_float zoom_speed{ 0.001, 0.001 };
	V2_float min_scale{ 0.1, 0.1 };
	V2_float max_scale{ 5, 5 };
};

} // namespace ptgn