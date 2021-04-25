#pragma once

#include "math/Vector2.h"

namespace engine {

struct Camera {
	Camera() = default;
	Camera(const V2_double& scale, V2_double zoom_speed = { 0.1, 0.1 }, V2_double scale_bound = { 5.0, 5.0 }, V2_double offset = {}) : scale{ scale }, zoom_speed{ zoom_speed }, scale_bound{ scale_bound }, offset{ offset } {}
	V2_double scale{ 1.0, 1.0 };
	V2_double zoom_speed{ 0.1, 0.1 };
	V2_double scale_bound{ 5.0, 5.0 };
	V2_double offset;
	void ClampToBound();
	void CenterOn(const V2_double& point, const V2_double& size, std::size_t display_index = 0);
};

} // namespace engine