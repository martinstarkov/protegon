#pragma once

#include <cstdint>

#include "renderer/render_target.h"

namespace ptgn {

// Information relating to the render layer and render target of the drawn object.
// Default constructed LayerInfo will automatically use the currently active scene.
struct LayerInfo {
	LayerInfo() = default;

	LayerInfo(void*) {}

	LayerInfo(const RenderTarget& render_target);

	// @param render_layer The render layer on which the object is drawn.
	// Higher values are closer to the camera, leading to objects being rendered on top of others.
	// Negative values are furthest away from camera.
	// @param render_target The render target which is used for rendering. Default value of {}
	// refers to the currently active scene.
	LayerInfo(std::int32_t render_layer, const RenderTarget& render_target = {});

	[[nodiscard]] bool operator==(const LayerInfo& o) const;

	[[nodiscard]] bool operator!=(const LayerInfo& o) const;

	[[nodiscard]] RenderTarget GetRenderTarget() const;

	[[nodiscard]] std::int32_t GetRenderLayer() const;

private:
	std::int32_t render_layer_{ 0 };
	RenderTarget render_target_;
};

} // namespace ptgn