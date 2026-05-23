#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/vertex/vertex.h"

namespace ptgn {

class Application;

namespace impl {

class Renderer;

struct TriangleCommand {
	std::vector<RenderTriangle<ColorVertex>> triangles;
};

struct QuadCommand {
	std::vector<RenderQuad<ColorVertex>> quads;
};

struct ShapeCommand {
	ShaderId shader;
	std::vector<RenderQuad<ShapeVertex>> quads;
};

struct TextureCommand {
	ShaderId shader;
	TextureId texture;
	std::vector<RenderQuad<TextureVertex>> quads;
};

using ManualCommand = std::variant<TriangleCommand, QuadCommand, ShapeCommand, TextureCommand>;

} // namespace impl

class DrawContext {
private:
	class StateScope {
	private:
		StateScope() = delete;

		StateScope(DrawContext& ctx, const RenderState& delta_state);

		StateScope(const StateScope&)			 = delete;
		StateScope& operator=(const StateScope&) = delete;

		StateScope(StateScope&&) noexcept			 = delete;
		StateScope& operator=(StateScope&&) noexcept = delete;

		~StateScope();

		friend class DrawContext;

		DrawContext& ctx_;
		RenderState previous_state_;
	};

public:
	void WithState(const RenderState& delta, InvocableR<void> auto&& function) {
		StateScope scope{ *this, delta };

		function();
	}

	void WithBlendMode(BlendMode blend_mode, InvocableR<void> auto&& function) {
		StateScope scope{ *this, RenderState{ .blend_mode{ blend_mode } } };

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

	void Draw(const impl::ManualCommand& cmd);

	impl::ShaderId GetShader(std::string_view name) const;

	// TODO: Move these to private once Scene renderer is added.
	void SetViewport(Viewport viewport);
	void SetViewProjection(const Matrix4& view_projection);
	void SetScissor(const ScissorState& scissor);
	void SetRenderTarget(const impl::RenderTargetObject* target);

private:
	friend class StateScope;
	friend class impl::Renderer;
	friend class Application;

	void SetRenderState(const RenderState& state);

	explicit DrawContext(impl::Renderer& renderer);

	impl::Renderer& renderer_;
};

} // namespace ptgn