#pragma once

#include "math/Vector2.h"

namespace ptgn {

struct Camera {
	Camera() = default;
	~Camera() = default;

	// Zoom camera in by the set zoom speed.
	void ZoomIn();
	void ZoomIn(const V2_double& amount);

	// Zoom camera out by the set zoom speed.
	void ZoomOut();
	void ZoomOut(const V2_double& amount);
	// Center camera on a point with a size.
	void CenterOn(const V2_double& point, const V2_double& size = {});

	V2_double RelativePosition(const V2_double& object_position);
	V2_double RelativeSize(const V2_double& object_size);
private:
	// Clamp camera zoom to minimum and maximum.
	void ClampZoom();

	V2_double position;
	V2_double scale{ 1.0, 1.0 };
	V2_double zoom_speed{ 0.001, 0.001 };
	V2_double min_scale = { 0.1, 0.1 };
	V2_double max_scale = { 5.0, 5.0 };
};

} // namespace ptgn