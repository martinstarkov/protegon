#pragma once

#include <functional>

#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "utility/handle.h"

namespace ptgn {

namespace impl {

class CameraManager;
class Renderer;
class InputHandler;

struct RenderTargetInstance {
	RenderTargetInstance()										 = default;
	RenderTargetInstance(RenderTargetInstance&&)				 = default;
	RenderTargetInstance& operator=(RenderTargetInstance&&)		 = default;
	RenderTargetInstance(const RenderTargetInstance&)			 = delete;
	RenderTargetInstance& operator=(const RenderTargetInstance&) = delete;
	~RenderTargetInstance()										 = default;

	explicit RenderTargetInstance(const Color& clear_color);
	RenderTargetInstance(const V2_float& size, const Color& clear_color);

	void Bind() const;

	Texture texture_;

	FrameBuffer frame_buffer_;

	Color clear_color_{ color::Transparent };

	// The camera of the render target. Set by the camera manager.
	Camera camera_;

	// Default {} results in window sized viewport.
	Rect viewport_;
};

} // namespace impl

// Constructing a RenderTarget object requires the engine to be initialized.
class RenderTarget : public Handle<impl::RenderTargetInstance> {
public:
	// A default render target will result in the screen being used as the render target.
	RenderTarget() = default;

	// Create a render target that is continuously sized to the window.
	// @param clear_color The background color of the render target.
	explicit RenderTarget(const Color& clear_color);

	// Create a render target with a custom size.
	// @param size The size of the render target.
	// @param clear_color The background color of the render target.
	RenderTarget(const V2_float& size, const Color& clear_color = color::Transparent);

	// @param viewport Where to draw the render target. Default value of {} results in
	// render texture sized viewport.
	void SetViewport(Rect viewport = {});

	// @return Viewport of the render target.
	[[nodiscard]] Rect GetViewport() const;

	// Transforms a window relative pixel coordinate to being relative to the target viewport and
	// primary game camera.
	// @param screen_coordinate The coordinate to be transformed.
	[[nodiscard]] V2_float ScreenToTarget(const V2_float& screen_coordinate) const;

	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param shader Specify a custom shader when drawing the render target. If {},
	// uses the default screen shader.
	void Draw(const TextureInfo& texture_info = {}, Shader shader = {}) const;

	// @return Texture associated with the render target.
	[[nodiscard]] const Texture& GetTexture() const;

	// WARNING: This is a very slow operation, and should not be used frequently. If reading pixels
	// from a regular texture, prefer to create a Surface{ path } and read pixels from that instead.
	// This function is primarily for debugging render targets.
	// @param coordinate Pixel coordinate from [0, size).
	// @return Color value of the given pixel.
	// Note: Only RGB/RGBA format textures supported.
	// Note: This will bind the render target's frame buffer.
	[[nodiscard]] Color GetPixel(const V2_int& coordinate) const;

	// WARNING: This is a very slow operation, and should not be used frequently. If reading pixels
	// from a regular texture, prefer to create a Surface{ path } and read pixels from that instead.
	// This function is primarily for debugging render targets.
	// @param callback Function to be called for each pixel.
	// Note: Only RGB/RGBA format textures supported.
	// Note: This will bind the render target's frame buffer.
	void ForEachPixel(const std::function<void(V2_int, Color)>& callback) const;

	[[nodiscard]] const Camera& GetCamera() const;

private:
	friend class Shader;
	friend class impl::Renderer;
	friend class impl::CameraManager;

	void SetCamera(const Camera& camera);

	[[nodiscard]] Color GetClearColor() const;
	void SetClearColor(const Color& clear_color);

	void Bind() const;
};

} // namespace ptgn