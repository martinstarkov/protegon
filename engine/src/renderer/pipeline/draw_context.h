#pragma once

#include <array>
#include <span>
#include <string_view>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"

namespace ptgn {

class Application;

namespace impl {

class Renderer;

} // namespace impl

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

	void DrawTexture(
		impl::TextureId texture, Transform transform, float depth, V2_float size,
		Origin draw_origin, Color tint, const std::array<V2_float, 4>& tex_coords,
		const impl::EffectParams& effects, int entity_id
	);

	void DrawTexture(
		const MaterialState& material, impl::TextureId texture, Transform transform, float depth,
		V2_float size, Origin draw_origin, Color tint, const std::array<V2_float, 4>& tex_coords,
		const impl::EffectParams& effects, int entity_id
	);

	void DrawShader(
		const MaterialState& material, Transform transform, float depth, V2_float size,
		Origin draw_origin, Color tint, const std::array<V2_float, 4>& tex_coords,
		const impl::EffectParams& effects, int entity_id
	);

	void DrawShape(
		const Shape& shape, Transform transform, float depth, Color tint, FillStyle fill_style,
		Origin draw_origin, int entity_id
	);

	void DrawLines(
		std::span<const V2_float> points, Transform transform, float depth, Color tint,
		float line_width, bool connect_last_to_first
	);

	impl::ShaderId GetShader(std::string_view name) const;

private:
	friend class RenderStateScope;
	friend class impl::Renderer;
	friend class Application;

	void SetRenderState(const RenderState& state);

	explicit DrawContext(impl::Renderer& renderer);

	impl::Renderer& renderer_;
};

} // namespace ptgn