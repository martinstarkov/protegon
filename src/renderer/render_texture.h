#pragma once

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/texture.h"
#include "scene/camera.h"

namespace ptgn {

struct Rect;

class RenderTexture : public Texture {
public:
	RenderTexture()			  = default;
	~RenderTexture() override = default;
	explicit RenderTexture(
		const V2_float& size, const Color& clear_color = color::Transparent,
		BlendMode blend_mode = BlendMode::Add
	);

	void Clear() const;

	// TODO: Move to private.
	void DrawAndUnbind();

	// TODO: Move to private.
	void Bind() const;

	[[nodiscard]] Color GetClearColor() const;
	void SetClearColor(const Color& clear_color);

	[[nodiscard]] BlendMode GetBlendMode() const;
	void SetBlendMode(BlendMode blend_mode);

private:
	friend class impl::Renderer;

	// TODO: Make this be a global camera variable.
	OrthographicCamera window_camera_;
	Color clear_color_{ color::Transparent };
	BlendMode blend_mode_{ BlendMode::Add };
	FrameBuffer frame_buffer_;
};

} // namespace ptgn