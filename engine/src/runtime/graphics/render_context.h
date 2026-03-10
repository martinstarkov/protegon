#pragma once

#include <array>
#include <optional>
#include <span>
#include <variant>

#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/text.h"

namespace ptgn {

class Scene;
class Renderer;

class RenderContext {
public:
	RenderContext()									   = default;
	~RenderContext() noexcept						   = default;
	RenderContext(const RenderContext&)				   = delete;
	RenderContext(RenderContext&&) noexcept			   = delete;
	RenderContext& operator=(const RenderContext&)	   = delete;
	RenderContext& operator=(RenderContext&&) noexcept = delete;

	void DrawTexture(
		Texture texture, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = {},
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = {},
		std::optional<Camera> camera									  = {}
	);

	void DrawTexture(
		Texture texture, Shader shader, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode								  = {},
		const std::optional<std::array<V2_float, 4>>& texture_coordinates = {},
		std::optional<Camera> camera									  = {}
	);

	void DrawShader(
		Shader shader, Transform transform, std::optional<V2_float> size = {},
		Origin draw_origin = Origin::Center, std::optional<Color> tint = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawLines(
		const std::vector<V2_float>& points, Color color, float line_width = kMinLineWidth,
		bool connect_last_to_first = false, std::optional<Transform> transform = {},
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		std::optional<Camera> camera = {}
	);

	void DrawShape(
		const Shape& shape, Transform transform, Color color, FillStyle fill_style,
		Origin draw_origin = Origin::Center, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawText(
		std::string_view content, Transform transform, Color text_color,
		std::optional<float> font_size									 = {},
		const std::variant<std::monostate, Font, std::string_view>& font = {},
		const TextProperties& properties = {}, Origin origin = Origin::Center,
		std::optional<V2_float> text_size = {}, bool hd_text = true, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawRect(
		Transform transform, const Rect& rect, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Origin origin = Origin::Center,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		std::optional<Camera> camera = {}
	);

	void DrawRoundedRect(
		Transform transform, const RoundedRect& rounded_rect, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Origin origin = Origin::Center,
		Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		std::optional<Camera> camera = {}
	);

	void DrawLine(
		Transform transform, const Line& line, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawLine(
		const V2_float& start, const V2_float& end, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawTriangle(
		Transform transform, const Triangle& triangle, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawEllipse(
		Transform transform, const Ellipse& ellipse, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawCircle(
		Transform transform, const Circle& circle, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawCapsule(
		Transform transform, const Capsule& capsule, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawArc(
		Transform transform, const Arc& arc, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawPolygon(
		Transform transform, const Polygon& polygon, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawPoint(
		V2_float point, Color color, Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		std::optional<Camera> camera = {}
	);

private:
	friend class Scene;

	void Init(Scene& scene, Renderer& renderer);

	Scene* scene_{ nullptr };
	Renderer* renderer_{ nullptr };
};

} // namespace ptgn