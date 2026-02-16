#pragma once

#include <optional>

#include "core/math/matrix4.h"
#include "renderer/camera/viewport.h"

namespace ptgn::impl {

struct Camera {
	Viewport viewport;

	// If true, rounds camera position to pixel precision.
	bool pixel_rounding{ false };

	// If nullopt, no bounds are enforced.
	std::optional<Viewport> bounding_box;

	Matrix4 view{ 1.0f };
	Matrix4 projection{ 1.0f };
	Matrix4 view_projection{ 1.0f };
};

} // namespace ptgn::impl