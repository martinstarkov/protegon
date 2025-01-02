#pragma once

#include "math/vector2.h"
#include "renderer/render_data.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "utility/handle.h"

namespace ptgn {

struct LayerInfo;
struct Rect;

namespace impl {

struct RenderTargetInstance {
	CameraManager camera_;
	RenderData render_data_;
	BlendMode blend_mode_;
	Color clear_color_;
	FrameBuffer frame_buffer_;
	Texture texture_;
};

} // namespace impl

// Constructing a RenderTarget object requires the engine to be initialized.
class RenderTarget : public Handle<impl::RenderTargetInstance> {
public:
	~RenderTarget() = default;

	// Continuously window sized.
	RenderTarget(
		const Color& clear_color = color::Transparent, BlendMode blend_mode = BlendMode::Blend
	);

	RenderTarget(
		const V2_float& size, const Color& clear_color = color::Transparent,
		BlendMode blend_mode = BlendMode::Blend
	);

	// TODO: Add screen shaders as options.

	// Uses default render target.
	void Draw(const Rect& destination);

	void Draw(const Rect& destination, const LayerInfo& layer_info);

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
};

} // namespace ptgn