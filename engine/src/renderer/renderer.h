#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/app/engine_context.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/effects.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/components/transform.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/material/texture.h"
#include "renderer/render_data.h"
#include "renderer/text/font.h"
#include "renderer/text/text.h"
#include "world/scene/camera.h"

namespace ptgn {

class Shader;
class Scene;
class RenderTarget;
class Shape;
class Entity;
struct Capsule;
struct Arc;
struct Circle;
struct Ellipse;
struct RoundedRect;
struct Rect;
struct Line;
struct Polygon;
struct Triangle;

class Renderer {
public:
	Renderer() = default;
	Renderer(EngineContext ctx, const V2_int& viewport_size);

	~Renderer() noexcept					 = default;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = default;

	void SetBackgroundColor(const Color& background_color);
	[[nodiscard]] Color GetBackgroundColor() const;

	// @param game_size Setting to {} will use window size.
	void SetGameSize(
		const V2_int& game_size = {}, ScalingMode scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	// @return The display size of the renderer.
	[[nodiscard]] V2_int GetDisplaySize() const;

	// @return The amount by which game size is scaled to achieve the display size.
	[[nodiscard]] V2_float GetScale() const;

	// @return The game size of the renderer.
	[[nodiscard]] V2_int GetGameSize() const;

	// @return The game size scaling mode.
	[[nodiscard]] ScalingMode GetScalingMode() const;

	void DrawTexture(
		const impl::Texture& texture, const Transform& transform, const V2_float& texture_size = {},
		Origin origin = default_origin, const Tint& tint = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PreFX& pre_fx = {}, const PostFX& post_fx = {},
		const std::array<V2_float, 4>& texture_coordinates = impl::GetDefaultTextureCoordinates()
	);

	void DrawTexture(
		const TextureHandle& texture_key, const Transform& transform,
		const V2_float& texture_size = {}, Origin origin = default_origin, const Tint& tint = {},
		const Depth& depth = {}, BlendMode blend_mode = default_blend_mode,
		const Camera& camera = {}, const PreFX& pre_fx = {}, const PostFX& post_fx = {},
		const std::array<V2_float, 4>& texture_coordinates = impl::GetDefaultTextureCoordinates()
	);

	void DrawLines(
		const Transform& transform, const std::vector<V2_float>& line_points, const Tint& color,
		const LineWidth& line_width = {}, bool connect_last_to_first = false,
		const Depth& depth = {}, BlendMode blend_mode = default_blend_mode,
		const Camera& camera = {}, const PostFX& post_fx = {}
	);

	void DrawLines(
		const std::vector<V2_float>& line_points, const Tint& color,
		const LineWidth& line_width = {}, bool connect_last_to_first = false,
		const Depth& depth = {}, BlendMode blend_mode = default_blend_mode,
		const Camera& camera = {}, const PostFX& post_fx = {}
	);

	// @param origin only applicable to Rect and RoundedRect.
	void DrawShape(
		const Transform& transform, const Shape& shape, const Tint& color,
		const LineWidth& line_width = {}, Origin origin = default_origin, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}, const impl::ShaderPass& shader_pass = {}
	);

	void DrawShader(
		const impl::ShaderPass& shader, const Entity& entity,
		bool clear_between_consecutive_calls	   = true,
		const Color& target_clear_color			   = color::Transparent,
		const impl::TextureOrSize& texture_or_size = V2_int{},
		BlendMode intermediate_blend_mode = default_blend_mode, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		TextureFormat texture_format = default_texture_format, const PostFX& post_fx = {},
		std::optional<BlendMode> target_blend_mode = std::nullopt
	);

	// @param text_size {} results in unscaled size of text based on font.
	void DrawText(
		const std::string& content, Transform transform, const TextColor& color,
		Origin origin = default_origin, const FontSize& font_size = {},
		const ResourceHandle& font_key = {}, const TextProperties& properties = {},
		V2_float text_size = {}, const Tint& tint = {}, bool hd_text = true,
		const Depth& depth = {}, BlendMode blend_mode = default_blend_mode,
		const Camera& camera = {}, const PreFX& pre_fx = {}, const PostFX& post_fx = {},
		const std::array<V2_float, 4>& texture_coordinates = impl::GetDefaultTextureCoordinates()
	);

	void DrawRect(
		const Transform& transform, const Rect& rect, const Tint& color,
		const LineWidth& line_width = {}, Origin origin = default_origin, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawRoundedRect(
		const Transform& transform, const RoundedRect& rounded_rect, const Tint& color,
		const LineWidth& line_width = {}, Origin origin = default_origin, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawLine(
		const Transform& transform, const Line& line, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawLine(
		const V2_float& start, const V2_float& end, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawTriangle(
		const Transform& transform, const Triangle& triangle, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawEllipse(
		const Transform& transform, const Ellipse& ellipse, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawCircle(
		const Transform& transform, const Circle& circle, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawCapsule(
		const Transform& transform, const Capsule& capsule, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawArc(
		const Transform& transform, const Arc& arc, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawPolygon(
		const Transform& transform, const Polygon& polygon, const Tint& color,
		const LineWidth& line_width = {}, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {},
		const PostFX& post_fx = {}
	);

	void DrawPoint(
		const V2_float& point, const Tint& color, const Depth& depth = {},
		BlendMode blend_mode = default_blend_mode, const Camera& camera = {}
	);

	void EnableStencilMask();
	void DisableStencilMask();
	void DrawOutsideStencilMask();
	void DrawInsideStencilMask();

	// TODO: Move everything below this to private.
	EngineContext ctx_;
	impl::RenderData render_data_;

	[[nodiscard]] impl::Texture CreateTexture(
		Transform& out_transform, V2_float& out_text_size, const TextContent& content,
		const TextColor& color, const FontSize& font_size, const ResourceHandle& font_key,
		const TextProperties& properties, bool hd_text, const Camera& camera
	);

	// Present the screen target to the window.
	void PresentScreen();

	// Clears the window buffer.
	void ClearScreen() const;

	// Renderer keeps track of what is bound.
	struct BoundStates {
		std::uint32_t frame_buffer_id{ 0 };
		std::uint32_t render_buffer_id{ 0 };
		std::uint32_t shader_id{ 0 };
		std::uint32_t vertex_array_id{ 0 };
		BlendMode blend_mode{ BlendMode::ReplaceRGBA };
		V2_int viewport_position;
		V2_int viewport_size;
	};

	BoundStates bound_;

private:
};

} // namespace ptgn