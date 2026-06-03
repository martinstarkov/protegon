#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

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
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"

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

struct ShapeDrawParams {
	float depth{ 0.0f };
	FillStyle fill_style{ 1.0f };
	Origin origin{ Origin::Center };
	int entity_id{ -1 };
	impl::EffectParams effects;
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

	void WithRenderTarget(
		impl::FramebufferObject* framebuffer, Viewport viewport, InvocableR<void> auto&& function
	) {
		auto previous_framebuffer{ &GetBoundFramebuffer() };
		auto previous_viewport{ GetRenderState().viewport };

		SetFramebuffer(framebuffer);
		SetViewport(viewport);

		function();

		SetFramebuffer(previous_framebuffer);

		if (previous_viewport.has_value()) {
			SetViewport(*previous_viewport);
		}
	}

	void WithBlendMode(BlendMode blend_mode, InvocableR<void> auto&& function) {
		RenderStateScope scope{ *this, RenderState{ .blend_mode{ blend_mode } } };

		function();
	}

	RenderState GetRenderState() const;

	void DrawTexture(Transform transform, impl::TextureId texture, TextureDrawParams params);

	void DrawTexture(
		Transform transform, impl::TextureId texture, const MaterialState& material,
		TextureDrawParams params
	);
	void DrawTexture(
		Transform transform, impl::TextureId texture, const Material& material,
		TextureDrawParams params
	);

	void DrawShader(Transform transform, const MaterialState& material, TextureDrawParams params);
	void DrawShader(Transform transform, const Material& material, TextureDrawParams params);

	void DrawPoint(V2_float point, Color color, const ShapeDrawParams& params);

	void DrawLine(
		V2_float start, V2_float end, Color color,
		const ShapeDrawParams& params = ShapeDrawParams{ .fill_style{ 1.0f } }
	);

	void DrawLines(
		std::span<const V2_float> points, Color color,
		const ShapeDrawParams& params = ShapeDrawParams{ .fill_style{ 1.0f } }, bool closed = false,
		std::optional<Transform> transform = std::nullopt
	);

	void DrawShape(
		Transform transform, const V2_float& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Rect& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const RoundedRect& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Polygon& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Triangle& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Capsule& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Line& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Arc& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Circle& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Ellipse& shape, Color color, const ShapeDrawParams& params
	);

	void DrawShape(
		Transform transform, const Shape& shape, Color color, const ShapeDrawParams& params
	);

	impl::ShaderId GetShader(std::string_view name) const;

	template <InvocableR<RenderPassHandle, RenderPassBuilder&> F>
	void Pass(F&& fn) {
		RenderPassBuilder render_pass_builder{ *this };

		auto final_handle{ std::invoke(std::forward<F>(fn), render_pass_builder) };

		render_pass_builder.Execute(final_handle);
	}

private:
	friend class RenderStateScope;
	friend class Renderer;
	friend class Application;
	friend class RenderPassBuilder;

	explicit DrawContext(Renderer& renderer);

	void DrawRenderPass(const impl::DrawPassRequest& request);

	[[nodiscard]] bool FramebufferPoolHas(impl::FramebufferId id) const;

	/// @brief NOTE: Caller is responsible for clearing the acquired framebuffer.
	[[nodiscard]] impl::FramebufferId AcquireFramebuffer(
		TextureDesc desc, std::optional<TextureDesc> other_desc
	);

	void ReleaseFramebuffer(impl::FramebufferId framebuffer);

	const impl::FramebufferObject& GetBoundFramebuffer() const;
	impl::FramebufferObject& GetBoundFramebuffer();

	void SetRenderState(const RenderState& state);

	void UpdateFramebuffer(impl::FramebufferObject&& replacing_framebuffer);

	V2_int GetSize(impl::FramebufferId framebuffer) const;
	TextureDesc GetDesc(impl::FramebufferId framebuffer) const;

	void CopyFramebufferRegion(
		impl::FramebufferId source, impl::FramebufferId destination, Viewport source_region,
		V2_int destination_position
	);

	void CompositeRenderPassResult(
		impl::FramebufferId source, impl::FramebufferId destination, Transform transform,
		const TextureDrawParams& params, const RenderState& state
	);

	void CompositeRenderPassResult(
		impl::FramebufferId source, impl::FramebufferId destination, Viewport destination_region
	);

	impl::FramebufferObject& GetPoolFramebuffer(impl::FramebufferId framebuffer);

	void SetFramebuffer(impl::FramebufferObject* framebuffer);
	void SetViewport(Viewport viewport);

	Renderer& renderer_;
};

} // namespace ptgn