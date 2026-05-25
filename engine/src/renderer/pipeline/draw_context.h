#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"

namespace ptgn {

class Application;
class Renderer;
class Rect;
class RoundedRect;
class Triangle;
class Polygon;
class Line;
class Circle;
class Ellipse;
class Capsule;
class Arc;
class Shape;

struct TextureDrawParams {
	float depth{ 0.0f };
	V2_float size;
	Origin origin{ Origin::Center };
	Color tint{ color::White };
	std::array<V2_float, 4> texture_coordinates;
	impl::EffectParams effects;
	int entity_id{ -1 };
};

struct ShapeDrawParams {
	float depth{ 0.0f };
	FillStyle fill_style{ 1.0f };
	Origin origin{ Origin::Center };
	int entity_id{ -1 };
};

class DrawContext {
private:
	class RenderStateScope {
	private:
		RenderStateScope() = delete;

		RenderStateScope(DrawContext& ctx, const RenderState& delta_state);

		RenderStateScope(const RenderStateScope&)			 = delete;
		RenderStateScope& operator=(const RenderStateScope&) = delete;

		RenderStateScope(RenderStateScope&&) noexcept			 = delete;
		RenderStateScope& operator=(RenderStateScope&&) noexcept = delete;

		~RenderStateScope();

		friend class DrawContext;

		DrawContext& ctx_;
		RenderState previous_state_;
	};

public:
	void WithRenderState(const RenderState& delta, InvocableR<void> auto&& function) {
		RenderStateScope scope{ *this, delta };

		function();
	}

	void WithBlendMode(BlendMode blend_mode, InvocableR<void> auto&& function) {
		RenderStateScope scope{ *this, RenderState{ .blend_mode{ blend_mode } } };

		function();
	}

	RenderState GetRenderState() const;

	void DrawTexture(Transform transform, impl::TextureId texture, TextureDrawParams params);

	void DrawTexture(
		Transform transform, impl::TextureId texture, MaterialState material,
		TextureDrawParams params
	);
	void DrawTexture(
		Transform transform, impl::TextureId texture, Material material, TextureDrawParams params
	);

	void DrawShader(Transform transform, MaterialState material, TextureDrawParams params);
	void DrawShader(Transform transform, Material material, TextureDrawParams params);

	void DrawPoint(V2_float point, Color color, ShapeDrawParams params);

	void DrawLine(
		V2_float start, V2_float end, Color color,
		ShapeDrawParams params = ShapeDrawParams{ .fill_style{ 1.0f } }
	);

	void DrawLines(
		std::span<const V2_float> points, Color color,
		ShapeDrawParams params = ShapeDrawParams{ .fill_style{ 1.0f } }, bool closed = false,
		std::optional<Transform> transform = std::nullopt
	);

	void DrawShape(Transform transform, const V2_float& shape, Color color, ShapeDrawParams params);

	void DrawShape(Transform transform, const Rect& shape, Color color, ShapeDrawParams params);

	void DrawShape(
		Transform transform, const RoundedRect& shape, Color color, ShapeDrawParams params
	);

	void DrawShape(Transform transform, const Polygon& shape, Color color, ShapeDrawParams params);

	void DrawShape(Transform transform, const Triangle& shape, Color color, ShapeDrawParams params);

	void DrawShape(Transform transform, const Capsule& shape, Color color, ShapeDrawParams params);

	void DrawShape(Transform transform, const Line& shape, Color color, ShapeDrawParams params);

	void DrawShape(Transform transform, const Arc& shape, Color color, ShapeDrawParams params);

	void DrawShape(Transform transform, const Circle& shape, Color color, ShapeDrawParams params);

	void DrawShape(Transform transform, const Ellipse& shape, Color color, ShapeDrawParams params);

	void DrawShape(Transform transform, const Shape& shape, Color color, ShapeDrawParams params);

	impl::ShaderId GetShader(std::string_view name) const;

private:
	friend class RenderStateScope;
	friend class Renderer;
	friend class Application;

	void SetRenderState(const RenderState& state);

	explicit DrawContext(Renderer& renderer);

	Renderer& renderer_;
};

} // namespace ptgn