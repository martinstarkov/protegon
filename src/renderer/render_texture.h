#pragma once

#include "camera/camera.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/texture.h"
#include "utility/handle.h"

namespace ptgn {

struct Rect;
class OrthographicCamera;

namespace impl {

class Renderer;

struct RenderTextureInstance {
	RenderTextureInstance() = default;

	~RenderTextureInstance();

	explicit RenderTextureInstance(
		bool continuously_window_sized, const V2_float& size, const Color& clear_color,
		BlendMode blend_mode
	);

	void Recreate(const V2_float& size, const Color& clear_color, BlendMode blend_mode);
	// Set to false by the renderer when any draw calls are flushed to the current render target.
	bool cleared_{ true };
	Color clear_color_{ color::Transparent };
	Texture texture_;
	FrameBuffer frame_buffer_;
	OrthographicCamera camera_;
	std::uint8_t opacity_{ 255 };
	BlendMode blend_mode_{ BlendMode::Add };
};

} // namespace impl

class RenderTexture : public Handle<impl::RenderTextureInstance> {
public:
	RenderTexture()			  = default;
	~RenderTexture() override = default;

	explicit RenderTexture(
		bool continuously_window_sized, const Color& clear_color = color::Transparent,
		BlendMode blend_mode = BlendMode::Add
	);
	explicit RenderTexture(
		const V2_float& size, const Color& clear_color = color::Transparent,
		BlendMode blend_mode = BlendMode::Add
	);

	void Clear();

	[[nodiscard]] V2_int GetSize() const;

	[[nodiscard]] Color GetClearColor() const;
	void SetClearColor(const Color& clear_color);

	// Value from 0 (transparent) to 255 (opaque).
	[[nodiscard]] std::uint8_t GetOpacity() const;

	// Value from 0 (transparent) to 255 (opaque).
	void SetOpacity(std::uint8_t opacity);

	[[nodiscard]] BlendMode GetBlendMode() const;
	void SetBlendMode(BlendMode blend_mode);

	[[nodiscard]] FrameBuffer GetFrameBuffer() const;
	[[nodiscard]] Texture GetTexture() const;

	void DrawAndUnbind(bool force_draw = false) const;
	void Bind() const;

	[[nodiscard]] OrthographicCamera GetCamera();

private:
	friend class impl::Renderer;

	void SetCleared(bool cleared);
};

} // namespace ptgn