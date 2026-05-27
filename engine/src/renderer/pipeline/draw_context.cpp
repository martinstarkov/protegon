#include "renderer/pipeline/draw_context.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "core/graphics/color.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/shape_primitives.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"

namespace ptgn {

namespace {

impl::CommonShapeParams ConvertToCommonShapeParams(
	Transform transform, Color color, const ShapeDrawParams& params
) {
	return { .transform{ transform },
			 .fill_style{ params.fill_style },
			 .draw_origin{ params.origin },
			 .color{ color },
			 .depth{ params.depth },
			 .entity_id{ params.entity_id } };
}

template <ShapeType TShape>
void DrawShapeImpl(
	Renderer& renderer, Transform transform, const TShape& shape, Color color,
	const ShapeDrawParams& params
) {
	impl::VisitPrimitives(
		shape, ConvertToCommonShapeParams(transform, color, params), [&](auto& primitives) {
			impl::RendererAccessor{ renderer }.Draw(primitives, impl::TextureId{});
		}
	);
}

} // namespace

DrawContext::DrawContext(Renderer& renderer) : renderer_{ renderer } {}

DrawContext::RenderStateScope::RenderStateScope(DrawContext& ctx, const RenderState& delta_state) :
	ctx_{ ctx }, previous_state_{ ctx_.GetRenderState() } {
	auto next_state{ impl::ApplyDeltaRenderState(previous_state_, delta_state) };
	ctx_.SetRenderState(next_state);
}

DrawContext::RenderStateScope::~RenderStateScope() {
	ctx_.SetRenderState(previous_state_);
}

RenderState DrawContext::GetRenderState() const {
	return renderer_.GetRenderState();
}

const impl::RenderTargetObject& DrawContext::GetRenderTarget() const {
	return renderer_.GetRenderTarget();
}

void DrawContext::SetRenderState(const RenderState& state) {
	renderer_.SetRenderState(state);
}

void DrawContext::UpdateRenderTarget(impl::RenderTargetObject&& replacing_target) {
	renderer_.UpdateRenderTarget(std::move(replacing_target));
}

void DrawContext::DrawTexture(
	Transform transform, impl::TextureId texture, TextureDrawParams params
) {
	DrawTexture(transform, texture, Material{ .shader = "texture" }, std::move(params));
}

void DrawContext::DrawTexture(
	Transform transform, impl::TextureId texture, MaterialState material, TextureDrawParams params
) {
	impl::DrawTextureRequest request;

	Rect rect{ params.size };

	request.texture = texture;

	auto local_vertices{ rect.GetLocalVertices() };
	auto local_quad{ impl::CreateTextureQuad(
		local_vertices, params.depth, params.tint.Normalized(), params.texture_coordinates,
		params.entity_id
	) };

	request.transform	  = rect.Offset(transform, params.origin);
	request.local_quads	  = { &local_quad, 1 };
	request.effect_params = params.effects;

	renderer_.SetCurrentPipeline("texture");
	renderer_.SetMaterial(material);
	renderer_.DrawTexture(request);
}

void DrawContext::DrawTexture(
	Transform transform, impl::TextureId texture, Material material, TextureDrawParams params
) {
	DrawTexture(
		transform, texture,
		MaterialState{ .shader	 = GetShader(material.shader),
					   .uniforms = std::move(material.uniforms) },
		std::move(params)
	);
}

void DrawContext::DrawShader(
	Transform transform, MaterialState material, TextureDrawParams params
) {
	DrawTexture(transform, impl::TextureId{}, std::move(material), std::move(params));
}

void DrawContext::DrawShader(Transform transform, Material material, TextureDrawParams params) {
	DrawTexture(transform, impl::TextureId{}, std::move(material), std::move(params));
}

void DrawContext::DrawPoint(V2_float point, Color color, ShapeDrawParams params) {
	DrawShape({}, point, color, std::move(params));
}

void DrawContext::DrawLine(V2_float start, V2_float end, Color color, ShapeDrawParams params) {
	DrawShape({}, Line{ start, end }, color, std::move(params));
}

void DrawContext::DrawLines(
	std::span<const V2_float> points, Color color, ShapeDrawParams params, bool closed,
	std::optional<Transform> transform
) {
	auto primitives{ impl::GetHollowPrimitives(
		points, closed, ConvertToCommonShapeParams(transform.value_or(Transform{}), color, params)
	) };

	renderer_.Draw<impl::RenderQuad<impl::ColorVertex>>(primitives, impl::TextureId{});
}

void DrawContext::DrawShape(
	Transform transform, const V2_float& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("color");
	renderer_.SetShader("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Rect& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("color");
	renderer_.SetShader("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const RoundedRect& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("shape");
	renderer_.SetShader("rounded_rect");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Polygon& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("color");
	renderer_.SetShader("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Triangle& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("color");
	renderer_.SetShader("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Capsule& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("shape");
	renderer_.SetShader("capsule");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Line& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("color");
	renderer_.SetShader("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Arc& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("shape");
	renderer_.SetShader("arc");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Circle& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("shape");
	renderer_.SetShader("ellipse");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Ellipse& shape, Color color, ShapeDrawParams params
) {
	renderer_.SetCurrentPipeline("shape");
	renderer_.SetShader("ellipse");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Shape& shape, Color color, ShapeDrawParams params
) {
	shape.Visit([&](const auto& specific_shape) {
		DrawShape(transform, specific_shape, color, std::move(params));
	});
}

impl::ShaderId DrawContext::GetShader(std::string_view name) const {
	return renderer_.GetShader(name);
}

bool DrawContext::RenderTargetPoolHas(impl::RenderTargetId id) const {
	return renderer_.target_pool_.Owns(id);
}

impl::RenderTargetId DrawContext::AcquireRenderTarget(RenderTargetDesc desc) {
	return renderer_.target_pool_.Acquire(desc);
}

void DrawContext::ReleaseRenderTarget(impl::RenderTargetId id) {
	return renderer_.target_pool_.Release(id);
}

impl::RenderTargetObject DrawContext::ExtractRenderTarget(impl::RenderTargetId id) {
	return renderer_.target_pool_.Extract(id);
}

void DrawContext::DrawRenderPass(
	impl::ShaderId shader, std::size_t pipeline, std::span<const impl::BoundInput> inputs,
	impl::RenderTargetId output
) {
	renderer_.DrawRenderPass(shader, pipeline, inputs, output);
}

} // namespace ptgn