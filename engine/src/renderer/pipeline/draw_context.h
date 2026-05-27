#pragma once

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"

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
class RenderPassBuilder;

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

	template <InvocableR<RenderPassHandle, RenderPassBuilder&> F>
	void Pass(F&& fn) {
		RenderPassBuilder render_pass_builder{ *this };

		auto final_handle{ std::invoke(std::forward<F>(fn), render_pass_builder) };

		auto final_target{ render_pass_builder.Execute(final_handle) };

		PTGN_ASSERT(final_target, "Final target must be a valid render target");

		UpdateRenderTarget(std::move(final_target));
	}

private:
	friend class RenderStateScope;
	friend class Renderer;
	friend class Application;
	friend class RenderPassBuilder;

	explicit DrawContext(Renderer& renderer);

	[[nodiscard]] bool RenderTargetPoolHas(impl::RenderTargetId id) const;
	[[nodiscard]] impl::RenderTargetId AcquireRenderTarget(RenderTargetDesc desc);
	void ReleaseRenderTarget(impl::RenderTargetId);

	const impl::RenderTargetObject& GetRenderTarget() const;

	void SetRenderState(const RenderState& state);

	void UpdateRenderTarget(impl::RenderTargetObject&& replacing_target);

	impl::RenderTargetObject ExtractRenderTarget(impl::RenderTargetId id);

	void DrawRenderPass(
		impl::ShaderId shader, std::size_t pipeline, std::span<const impl::BoundInput> inputs,
		impl::RenderTargetId output
	);

	Renderer& renderer_;
};

} // namespace ptgn