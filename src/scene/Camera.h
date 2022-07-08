#pragma once

#include "core/ECS.h"
#include "math/Vector2.h"

namespace ptgn {

struct Camera {
	Camera() = default;
	~Camera() = default;
	Camera(const V2_double& scale,
		   V2_double zoom_speed = { 0.1, 0.1 },
		   V2_double min_scale = { 0.1, 0.1 },
		   V2_double max_scale = { 5.0, 5.0 });

	// Clamp camera zoom to minimum and maximum.
	void ClampToBound();
	
	// Center camera on a point with a size.
	void CenterOn(const V2_double& point, V2_double size = {});

	/*
	* Center camera on an entity that has TransformComponent.
	* If entity has ShapeComponent and the use_size boolean is set to true, its size will be used.
	*/
	void CenterOn(const ecs::Entity& entity, bool use_size = true);

	// Zoom camera in by the set zoom speed.
	void ZoomIn();
	void ZoomIn(const V2_double& amount);

	// Zoom camera out by the set zoom speed.
	void ZoomOut();
	void ZoomOut(const V2_double& amount);

	V2_double position;
	V2_double scale{ 1.0, 1.0 };
	V2_double zoom_speed{ 0.1, 0.1 };
	V2_double min_scale = { 0.1, 0.1 };
	V2_double max_scale = { 5.0, 5.0 };
};

} // namespace ptgn