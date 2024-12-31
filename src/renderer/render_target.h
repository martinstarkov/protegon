#pragma once

#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/batch.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/layer_info.h"
#include "renderer/texture.h"
#include "scene/camera.h"

namespace ptgn {

class RenderTarget {
public:
	// Continuously window sized.
	RenderTarget(
		const Color& clear_color = color::Transparent, BlendMode blend_mode = BlendMode::Blend
	);
	RenderTarget(
		const V2_float& size, const Color& clear_color = color::Transparent,
		BlendMode blend_mode = BlendMode::Blend
	);

	void Draw(const Rect& destination = {}, ..., const LayerInfo& layer_info = {});

	// Set color to which render target is cleared.
	void SetClearColor();
	// @return The currently set clear color for the render target.
	[[nodiscard]] Color GetClearColor() const;

	// Flushes the render target's batch onto its frame buffer.
	void SetBlendMode();
	// @return The currently set blend mode for the render target.
	[[nodiscard]] BlendMode GetBlendMode() const;

	// Manually flushes the render target's batch onto its frame buffer.
	void Flush();

	// Manually clear the render target to its set clear color.
	void Clear();

private:
	void Bind();

	impl::CameraManager camera_;
	impl::Batch batch_;
	BlendMode blend_mode_;
	Color clear_color_;
	FrameBuffer frame_buffer_;
	Texture texture_;
};

} // namespace ptgn