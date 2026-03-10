#pragma once

#include <array>
#include <optional>
#include <span>

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
		std::span<const V2_float> points, Color color, float line_width,
		std::optional<Transform> transform = {}, Depth depth = {},
		std::optional<BlendMode> blend_mode = {}, std::optional<Camera> camera = {}
	);

	void DrawShape(
		const Shape& shape, Transform transform, Color color, FillStyle fill_style,
		Origin draw_origin, Depth depth = {}, std::optional<BlendMode> blend_mode = {},
		std::optional<Camera> camera = {}
	);

private:
	friend class Scene;

	void Init(Scene& scene, Renderer& renderer);

	Scene* scene_{ nullptr };
	Renderer* renderer_{ nullptr };
};

} // namespace ptgn