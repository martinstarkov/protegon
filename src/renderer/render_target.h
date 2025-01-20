#pragma once

#include <cstdint>

#include "math/geometry/polygon.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "utility/handle.h"

namespace ptgn {

struct LayerInfo;

namespace impl {

struct RenderTargetInstance {
	RenderTargetInstance() = default;
	RenderTargetInstance(const Color& clear_color, BlendMode blend_mode);
	RenderTargetInstance(const V2_float& size, const Color& clear_color, BlendMode blend_mode);
	RenderTargetInstance(RenderTargetInstance&&)				 = default;
	RenderTargetInstance& operator=(RenderTargetInstance&&)		 = default;
	RenderTargetInstance(const RenderTargetInstance&)			 = delete;
	RenderTargetInstance& operator=(const RenderTargetInstance&) = delete;
	~RenderTargetInstance()										 = default;

	void Flush();

	void Bind() const;

	void Clear() const;
	void SetClearColor(const Color& clear_color);
	void SetBlendMode(BlendMode blend_mode);

	Texture texture_;

	FrameBuffer frame_buffer_;

	BlendMode blend_mode_{ BlendMode::Blend };
	Color clear_color_{ color::Transparent };

	CameraManager camera_;

	RenderData render_data_;

	// Default value of {} corresponds to fullscreen.
	Rect destination_;
};

} // namespace impl

// Constructing a RenderTarget object requires the engine to be initialized.
class RenderTarget : public Handle<impl::RenderTargetInstance> {
public:
	RenderTarget() = default;
	// Create a render target that is continuously sized to the window.
	// @param clear_color The background color of the render target.
	// @param blend_mode The blend mode of the render target (i.e. how objects are drawn to it).
	RenderTarget(const Color& clear_color, BlendMode blend_mode = BlendMode::Blend);

	// Create a render target with a custom size.
	// @param size The size of the render target.
	// @param clear_color The background color of the render target.
	// @param blend_mode The blend mode of the render target (i.e. how objects are drawn to it).
	RenderTarget(
		const V2_float& size, const Color& clear_color = color::Transparent,
		BlendMode blend_mode = BlendMode::Blend
	);

	// @param target_destination Set the destination rectangle to which the render target is drawn.
	// Default value of {} corresponds to fullscreen.
	void SetRect(const Rect& target_destination);

	// @return The destination rectangle to which the render target is drawn.
	[[nodiscard]] Rect GetRect() const;

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

	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param shader Specify a custom shader when drawing the render targe to the screen. If {},
	// uses the default screen shader. Uses the default render target, which is the currently active
	// scene.
	void Draw(const TextureInfo& texture_info = {}, const Shader& shader = {});

	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param layer_info Information relating to the render layer and render target of the texture.
	// @param shader Specify a custom shader when drawing the render targe to the screen. If {},
	// uses the default screen shader.
	void Draw(
		const TextureInfo& texture_info, const LayerInfo& layer_info, const Shader& shader = {}
	);

	// @param clear_color The color to which the render target will be cleared.
	// Note: The change in clear color is only seen after a render target is cleared. This can
	// happen either manually by calling Clear() or automatically after drawing the render target
	// using Draw().
	void SetClearColor(const Color& clear_color);

	// @return The currently set clear color for the render target.
	[[nodiscard]] Color GetClearColor() const;

	// @param blend_mode The blend mode of the render target (i.e. how objects are drawn to it).
	void SetBlendMode(BlendMode blend_mode);

	// @return The currently set blend mode for the render target.
	[[nodiscard]] BlendMode GetBlendMode() const;

	// Manually flushes the render target's batch onto its frame buffer.
	void Flush();

	// Manually clear the render target to its set clear color.
	void Clear() const;

	// @return Texture associated with the render target.
	[[nodiscard]] Texture GetTexture() const;

	// @return CameraManager associated with the render target.
	[[nodiscard]] CameraManager& GetCamera();
	[[nodiscard]] const CameraManager& GetCamera() const;

	// Converts a coordinate from being relative to the screen to being relative to the render
	// target's primary camera.
	// @param coordinate The coordinate to be converted to target relative space.
	[[nodiscard]] V2_float ScreenToTarget(const V2_float& coordinate) const;

	// @return Mouse position scaled relative to the primary camera size of the render target.
	[[nodiscard]] V2_float GetMousePosition() const;

	// @return Mouse position during the previous frame scaled relative to the primary camera size
	// of the render target.
	[[nodiscard]] V2_float GetMousePositionPrevious() const;

	// @return Mouse position difference between the current and previous frames scaled relative to
	// the primary camera size of the render target.
	[[nodiscard]] V2_float GetMouseDifference() const;

	// @return Render data for the render target.
	[[nodiscard]] impl::RenderData& GetRenderData();
	[[nodiscard]] const impl::RenderData& GetRenderData() const;

private:
	friend class impl::Renderer;

	// @param post_flush Potential callback after screen target is flushed. For instance, for
	// setting custom viewport for letterboxing.
	void DrawToScreen(const std::function<void()>& post_flush = nullptr);

	void Bind() const;
};

} // namespace ptgn