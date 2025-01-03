#pragma once

#include <cstdint>

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/render_data.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "utility/handle.h"

namespace ptgn {

struct LayerInfo;
struct Rect;
struct Circle;
struct Ellipse;
struct Line;
struct Triangle;
struct RoundedRect;
struct Arc;
struct Capsule;
struct Polygon;

namespace impl {

struct Point;
class Renderer;
class SceneManager;
class SceneCamera;

struct RenderTargetInstance {
	RenderTargetInstance() = default;
	RenderTargetInstance(const Color& clear_color, BlendMode blend_mode);
	RenderTargetInstance(const V2_float& size, const Color& clear_color, BlendMode blend_mode);

	~RenderTargetInstance() = default;

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
};

} // namespace impl

// Constructing a RenderTarget object requires the engine to be initialized.
class RenderTarget : public Handle<impl::RenderTargetInstance> {
public:
	RenderTarget()	= default;
	~RenderTarget() = default;

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

	// TODO: Add screen shaders as options.

	// @param destination The rectangle destination to which the render target is drawn. Default
	// value of {} corresponds to fullscreen.
	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// Uses the default render target, which is the currently active scene.
	void Draw(const Rect& destination = {}, const TextureInfo& texture_info = {});

	// @param destination The rectangle destination to which the render target is drawn. {}
	// corresponds to fullscreen.
	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param layer_info Information relating to the render layer and render target of the texture.
	void Draw(
		const Rect& destination, const TextureInfo& texture_info, const LayerInfo& layer_info
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

private:
	friend class impl::Renderer;
	friend class impl::SceneManager;
	friend class impl::SceneCamera;
	friend struct LayerInfo;
	friend class Texture;
	friend struct Ellipse;
	friend struct Line;
	friend struct impl::Point;
	friend struct Triangle;
	friend struct Rect;
	friend struct Circle;
	friend struct RoundedRect;
	friend struct Arc;
	friend struct Capsule;
	friend struct Polygon;

	// @return Mouse relative to the window and the render target's primary camera size (zoom
	// included).
	[[nodiscard]] V2_float ScaleToWindow(const V2_float& position) const;

	// @param render_target The render target to retrieve the render layer for.
	// @return The render target corresponding to the correct rendering layer.
	// If (*this) render target is valid, it is the current rendering layer.
	// Otherwise, the currently active scene is the current rendering layer.
	// If no scene is currently active, an assert will be triggered.
	[[nodiscard]] static RenderTarget GetCorrectRenderLayer(const RenderTarget& render_target);

	// Draws this render target to another render target.
	// @param destination The destination rectangle to draw this target at.
	// @param texture_info Information relating to the source pixels, flip, tinting and rotation
	// center of the texture associated with this render target.
	// @param destination_target The target onto which this target is drawn.
	void DrawToTarget(
		const Rect& destination, const TextureInfo& texture_info, RenderTarget destination_target
	) const;

	void Bind();

	void AddTexture(
		const Texture& texture, const Rect& destination, const TextureInfo& texture_info,
		std::int32_t render_layer
	);

	void AddEllipse(
		const Ellipse& ellipse, const Color& color, float line_width, float fade,
		std::int32_t render_layer
	);

	void AddCircle(
		const Circle& circle, const Color& color, float line_width, float fade,
		std::int32_t render_layer
	);

	void AddLine(const Line& line, const Color& color, float line_width, std::int32_t render_layer);

	void AddPoint(
		const V2_float& point, const Color& color, float radius, float fade,
		std::int32_t render_layer
	);

	void AddTriangle(
		const Triangle& triangle, const Color& color, float line_width, std::int32_t render_layer
	);

	void AddRect(
		const Rect& rect, const Color& color, float line_width, std::int32_t render_layer,
		const V2_float& rotation_center
	);

	void AddRoundedRect(
		const RoundedRect& rrect, const Color& color, float line_width, float fade,
		std::int32_t render_layer, const V2_float& rotation_center
	);

	void AddArc(
		const Arc& arc, bool clockwise, const Color& color, float line_width, float fade,
		std::int32_t render_layer
	);

	void AddCapsule(
		const Capsule& capsule, const Color& color, float line_width, float fade,
		std::int32_t render_layer
	);

	void AddPolygon(
		const Polygon& polygon, const Color& color, float line_width, std::int32_t render_layer
	);
};

} // namespace ptgn