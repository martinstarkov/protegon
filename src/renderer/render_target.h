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

class Texture;

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
	~RenderTargetInstance();

	RenderTargetInstance(const Color& clear_color, BlendMode blend_mode);
	RenderTargetInstance(const V2_float& size, const Color& clear_color, BlendMode blend_mode);

	// Subscribes viewport to being resized to window size.
	// Will also set the viewport to the current window size.
	void SubscribeToEvents();

	// Unsubscribes viewport from being resized to window size.
	void UnsubscribeFromEvents() const;

	// Only to be used by the renderer screen target. Draws screen target to the screen frame buffer
	// (id=0) with the default screen shader.
	void DrawToScreen() const;

	// @param shader {} will result in default screen shader being used.
	void Draw(const TextureInfo& texture_info, Shader shader, bool clear_after_draw) const;

	[[nodiscard]] const Rect& GetViewport() const;
	void SetViewport(const Rect& viewport);

	[[nodiscard]] Camera& GetCamera();
	[[nodiscard]] const Camera& GetCamera() const;
	void SetCamera(const Camera& camera);

	[[nodiscard]] Color GetClearColor() const;
	void SetClearColor(const Color& clear_color);

	[[nodiscard]] BlendMode GetBlendMode() const;
	void SetBlendMode(BlendMode blend_mode);

	[[nodiscard]] const Texture& GetTexture() const;

	void Bind() const;
	void Clear() const;

	[[nodiscard]] V2_float ScreenToTarget(const V2_float& screen_coordinate) const;

	[[nodiscard]] Color GetPixel(const V2_int& coordinate) const;

	void ForEachPixel(const std::function<void(V2_int, Color)>& callback) const;

	Texture texture_;

	FrameBuffer frame_buffer_;

	BlendMode blend_mode_{ BlendMode::BlendPremultiplied };

	Color clear_color_{ color::Transparent };

	// Default {} results in window sized viewport.
	Rect viewport_;

	// The camera of the render target. Set by the camera manager.
	Camera camera_;
};

} // namespace impl

// Constructing a RenderTarget object requires the engine to be initialized.
// Each render target is initialized with a window camera. This can be changed by first setting the
// render target as current with game.renderer.SetRenderTarget(your_target) and then using
// game.camera.SetPrimary(your_camera) to set the camera for your_target.
class RenderTarget : public Handle<impl::RenderTargetInstance> {
public:
	// A default render target will result in the screen being used as the render target.
	RenderTarget() = default;

	// Create a render target that is continuously sized to the window.
	// @param clear_color The background color of the render target.
	// @param blend_mode The blend mode of the render target, i.e. how objects drawn to it are
	// blended.
	explicit RenderTarget(
		const Color& clear_color, BlendMode blend_mode = BlendMode::BlendPremultiplied
	);

	// Create a render target with a custom size.
	// @param size The size of the render target.
	// @param clear_color The background color of the render target.
	// @param blend_mode The blend mode of the render target, i.e. how objects drawn to it are
	// blended.
	RenderTarget(
		const V2_float& size, const Color& clear_color = color::Transparent,
		BlendMode blend_mode = BlendMode::BlendPremultiplied
	);

	// Transforms a window relative pixel coordinate to being relative to the target viewport and
	// primary game camera.
	// @param screen_coordinate The coordinate to be transformed.
	[[nodiscard]] V2_float ScreenToTarget(const V2_float& screen_coordinate) const;

	// @return Texture attaached to the render target.
	[[nodiscard]] const Texture& GetTexture() const;

	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param shader Specify a custom shader when drawing the render target. If {},
	// uses the default screen shader.
	// @param clear_after_draw If true, clears the render target after drawing it.
	void Draw(
		const TextureInfo& texture_info = {}, const Shader& shader = {},
		bool clear_after_draw = true
	) const;

	// Clear the render target.
	void Clear() const;

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

private:
	friend class Shader;
	friend class impl::Renderer;
	friend class impl::CameraManager;
	friend class impl::InputHandler;
	friend class Texture;
};

} // namespace ptgn