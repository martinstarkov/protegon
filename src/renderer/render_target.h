#pragma once

#include <cstdint>

#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/render_data.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "utility/handle.h"

namespace ptgn {

struct LayerInfo;

namespace impl {

class Renderer;

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

	// TODO: Add screen shaders as options.

	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// Uses the default render target, which is the currently active scene.
	void Draw(const TextureInfo& texture_info = {});

	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param layer_info Information relating to the render layer and render target of the texture.
	void Draw(const TextureInfo& texture_info, const LayerInfo& layer_info);

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

	// Converts a coordinate from being relative to the world to being relative to the render
	// target's primary camera.
	// @param position The position to be converted to screen space.
	[[nodiscard]] V2_float WorldToScreen(const V2_float& position) const;

	// Converts a coordinate from being relative to render target's primary camera to being relative
	// to the world.
	// @param position The position to be converted to world space.
	[[nodiscard]] V2_float ScreenToWorld(const V2_float& position) const;

	// Scales a size from being relative to the world to being relative to the render
	// target's primary camera.
	// @param size The size to be scaled to screen space.
	[[nodiscard]] V2_float ScaleToScreen(const V2_float& size) const;

	// Scales a size from being relative to the world to being relative to the render
	// target's primary camera.
	// @param size The size to be scaled to screen space.
	[[nodiscard]] float ScaleToScreen(float size) const;

	// Scales a size from being relative to the render target's primary camera to being relative to
	// world space.
	// @param size The size to be scaled to world space.
	[[nodiscard]] V2_float ScaleToWorld(const V2_float& size) const;

	// Scales a size from being relative to the render target's primary camera to being relative to
	// world space.
	// @param size The size to be scaled to world space.
	[[nodiscard]] float ScaleToWorld(float size) const;

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

	// @return Mouse relative to the window and the render target's primary camera size (zoom
	// included).
	[[nodiscard]] V2_float ScaleToWindow(const V2_float& position) const;

	// Draws this render target to another render target.
	// @param destination The destination rectangle to draw this target at.
	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param layer_info The target layer onto which this target is drawn.
	void DrawToTarget(
		const Rect& destination, const TextureInfo& texture_info, const LayerInfo& layer_info
	) const;

	void Bind();
};

} // namespace ptgn