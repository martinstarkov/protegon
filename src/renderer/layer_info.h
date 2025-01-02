#pragma once

#include <cstddef>

#include "renderer/render_target.h"

namespace ptgn {

// Information relating to the z index and render target of the texture.
// Defaults to currently active scene.
struct LayerInfo {
	LayerInfo() = default;

	explicit LayerInfo(const RenderTarget& render_target) : render_target{ render_target } {}

	// @param render_layer The render layer on which the object is drawn.
	// Higher values are closer to the camera, leading to objects being rendered on top of others.
	// Negative values are furthest away from camera.
	// @param render_target The render target which is used for rendering. Default value of {}
	// refers to the currently active scene.
	LayerInfo(std::int32_t render_layer, const RenderTarget& render_target = {}) :
		z_index{ z_index }, render_target{ render_target } {}

	std::int32_t render_layer{ 0 };
	RenderTarget render_target;
};

} // namespace ptgn