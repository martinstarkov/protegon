#include "renderer/draw_context.h"

#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/log.h"
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
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/framebuffer_pool.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_request.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/shape_primitives.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/text/text_layout.h"

namespace ptgn {

namespace {

impl::CommonShapeParams ConvertToCommonShapeParams(
	Transform transform, Color color, const ShapeDrawParams& params
) {
	return { .transform{ transform },
			 .fill_style{ params.fill_style },
			 .origin = params.origin,
			 .color{ color },
			 .depth{ params.depth },
			 .entity_id = params.entity_id };
}

template <ShapeType TShape>
void DrawShapeImpl(
	Renderer& renderer, Transform transform, const TShape& shape, Color color,
	const ShapeDrawParams& params
) {
	impl::VisitPrimitives(
		shape, ConvertToCommonShapeParams(transform, color, params),
		[&renderer, &params](auto& primitives) {
			if (primitives.empty()) {
				return;
			}

			using TPrimitive = std::remove_reference_t<decltype(primitives[0])>;

			impl::DrawRequest<TPrimitive> request{
				.transform      = {},
				.primitives     = primitives,
				.effect_params = params.effects,
			};

			impl::RendererAccessor{ renderer }.Draw(request);
		}
	);
}

} // namespace

DrawContext::DrawContext(Renderer& renderer) : renderer_{ renderer } {}

DrawContext::RenderStateScope::RenderStateScope(DrawContext& ctx, const RenderStateDelta& delta) :
	ctx_{ ctx }, previous_state_{ ctx_.GetRenderState() } {
	ctx_.SetRenderStateDelta(delta);
}

DrawContext::RenderStateScope::~RenderStateScope() {
	ctx_.SetRenderState(previous_state_);
}

DrawContext::RenderTargetScope::RenderTargetScope(DrawContext& ctx) :
	ctx_{ ctx },
	previous_framebuffer_{ &ctx.GetBoundFramebuffer() },
	previous_viewport_{ ctx.GetRenderState().viewport },
	previous_scissor_{ ctx.GetRenderState().scissor } {}

DrawContext::RenderTargetScope::~RenderTargetScope() {
	ctx_.SetFramebuffer(previous_framebuffer_);
	ctx_.SetViewport(previous_viewport_);
	ctx_.SetScissor(previous_scissor_);
}

DrawContext::TemporaryFramebufferScope::TemporaryFramebufferScope(
	DrawContext& ctx, TextureDesc desc, const std::optional<TextureDesc>& other_desc
) :
	ctx_{ ctx }, framebuffer_{ ctx_.AcquireFramebuffer(desc, other_desc) } {}

DrawContext::TemporaryFramebufferScope::~TemporaryFramebufferScope() {
	ctx_.renderer_.FlushBatch();
	ctx_.ReleaseFramebuffer(framebuffer_);
}

impl::FramebufferObject& DrawContext::TemporaryFramebufferScope::Get() {
	return ctx_.GetPoolFramebuffer(framebuffer_);
}

impl::FramebufferId DrawContext::TemporaryFramebufferScope::GetId() const {
	return framebuffer_;
}

void DrawContext::SetBlendMode(BlendMode blend_mode) {
	renderer_.SetBlendMode(blend_mode);
}

void DrawContext::DrawText(
	Transform transform, const DrawTextRequest& request, const impl::EffectParams& effects
) {
	auto text_batches{ impl::BuildTextDrawBatches(request) };

	if (text_batches.empty()) {
		return;
	}

	for (auto& batch : text_batches) {
		if (batch.quads.empty()) {
			continue;
		}

		MaterialState material{ .shader = GetShader("text"),
								.uniforms{
									impl::GetTextUniforms(batch.style.sdf, batch.decoration),
								},
								.texture_slot_capacity = GetMaxTextureSlots() };

		impl::DrawTextureRequest texture_request{
			.texture	   = batch.style.texture,
			.transform	   = transform,
			.primitives	   = batch.quads,
			.effect_params = effects,
		};

		renderer_.SetCurrentPipeline("texture");
		renderer_.SetMaterial(material);
		renderer_.DrawTexture(texture_request);
	}
}

RenderState DrawContext::GetRenderState() const {
	return renderer_.GetRenderState();
}

const impl::FramebufferObject& DrawContext::GetBoundFramebuffer() const {
	return renderer_.GetBoundFramebuffer();
}

impl::FramebufferObject& DrawContext::GetBoundFramebuffer() {
	return renderer_.GetBoundFramebuffer();
}

void DrawContext::SetRenderStateDelta(const RenderStateDelta& delta) {
	renderer_.SetRenderStateDelta(delta);
}

void DrawContext::SetRenderState(const RenderState& state) {
	renderer_.SetRenderState(state);
}

std::size_t DrawContext::GetMaxTextureSlots() const {
	return renderer_.GetMaxTextureSlots();
}

void DrawContext::DrawTexture(
	const impl::DrawTextureRequest& request, const MaterialState& material
) {
	renderer_.SetCurrentPipeline("texture");
	renderer_.SetMaterial(material);
	renderer_.DrawTexture(request);
}

void DrawContext::DrawTexture(
	Transform transform, impl::TextureId texture, const MaterialState& material,
	TextureDrawParams params
) {
	impl::DrawTextureRequest request;

	if (params.size.IsZero()) {
		auto size{ renderer_.GetSize(texture) };

		if (!size.has_value()) {
			PTGN_WARN("Texture size not found for texture ", texture, ", skipping draw");
			return;
		}

		params.size = size.value();
	}

	if (!params.size.IsPositive()) {
		PTGN_WARN("Texture size is not positive, skipping draw");
		return;
	}

	Rect rect{ params.size };

	request.texture = texture;

	auto local_vertices{ rect.GetLocalVertices() };
	auto local_quad{ impl::CreateTextureQuad(
		local_vertices, params.depth, params.tint.Normalized(), params.texture_coordinates,
		params.entity_id
	) };

	request.transform	  = rect.Offset(transform, params.origin);
	request.primitives	  = { &local_quad, 1 };
	request.effect_params = params.effects;

	DrawTexture(request, material);
}

void DrawContext::DrawTexture(
	Transform transform, impl::TextureId texture, TextureDrawParams params
) {
	DrawTexture(
		transform, texture,
		MaterialState{ .shader = GetShader("texture"), .texture_slot_capacity = renderer_.GetMaxTextureSlots() },
		std::move(params)
	);
}

void DrawContext::DrawShader(
	Transform transform, const MaterialState& material, TextureDrawParams params
) {
	DrawTexture(transform, impl::TextureId{}, material, std::move(params));
}

void DrawContext::DrawPoint(V2_float point, Color color, const ShapeDrawParams& params) {
	DrawShape({}, point, color, params);
}

void DrawContext::DrawLine(
	V2_float start, V2_float end, Color color, const ShapeDrawParams& params
) {
	DrawShape({}, Line{ start, end }, color, params);
}

void DrawContext::DrawLines(
	std::span<const V2_float> points, Color color, const ShapeDrawParams& params, bool closed,
	std::optional<Transform> transform
) {
	auto draw_transform{ transform.value_or(Transform{}) };
	auto primitives{ impl::GetHollowPrimitives(
		points, closed, ConvertToCommonShapeParams(draw_transform, color, params)
	) };

	impl::DrawRequest<impl::ColorQuad> request{
		.transform  = {},
		.primitives = primitives,
	};

	renderer_.SetCurrentPipeline("color");
	SetMaterial("color");

	renderer_.Draw(request);
}

void DrawContext::SetMaterial(std::string_view shader) {
	renderer_.SetMaterial(
		MaterialState{
			.shader = GetShader(shader),
		}
	);
}

void DrawContext::DrawShape(
	Transform transform, const V2_float& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("color");
	SetMaterial("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Rect& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("color");
	SetMaterial("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const RoundedRect& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("shape");
	SetMaterial("rounded_rect");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Polygon& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("color");
	SetMaterial("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Triangle& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("color");
	SetMaterial("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Capsule& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("shape");
	SetMaterial("capsule");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Line& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("color");
	SetMaterial("color");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Arc& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("shape");
	SetMaterial("arc");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Circle& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("shape");
	SetMaterial("ellipse");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Ellipse& shape, Color color, const ShapeDrawParams& params
) {
	renderer_.SetCurrentPipeline("shape");
	SetMaterial("ellipse");
	DrawShapeImpl(renderer_, transform, shape, color, params);
}

void DrawContext::DrawShape(
	Transform transform, const Shape& shape, Color color, const ShapeDrawParams& params
) {
	shape.Visit([this, transform, color, &params](const auto& specific_shape) {
		DrawShape(transform, specific_shape, color, params);
	});
}

impl::ShaderId DrawContext::GetShader(std::string_view name) const {
	return renderer_.GetShader(name);
}

bool DrawContext::FramebufferPoolHas(impl::FramebufferId framebuffer) const {
	return renderer_.framebuffer_pool_.Owns(framebuffer);
}

impl::FramebufferId DrawContext::AcquireFramebuffer(
	TextureDesc desc, const std::optional<TextureDesc>& other_desc
) {
	return renderer_.framebuffer_pool_.Acquire(desc, other_desc);
}

void DrawContext::ReleaseFramebuffer(impl::FramebufferId framebuffer) {
	return renderer_.framebuffer_pool_.Release(framebuffer);
}

std::optional<V2_int> DrawContext::GetSize(impl::FramebufferId framebuffer) const {
	return renderer_.GetSize(framebuffer);
}

std::optional<TextureDesc> DrawContext::GetDesc(impl::FramebufferId framebuffer) const {
	return renderer_.GetDesc(framebuffer);
}

void DrawContext::DrawRenderPass(const impl::DrawPassRequest& request) {
	renderer_.DrawRenderPass(request);
}

void DrawContext::CopyFramebufferRegion(
	impl::FramebufferId source, impl::FramebufferId destination, Viewport source_region,
	V2_int destination_position
) {
	renderer_.CopyFramebufferRegion(source, destination, source_region, destination_position);
}

void DrawContext::CompositeRenderPassResult(
	impl::FramebufferId color_source,
	impl::FramebufferId destination,
	Viewport destination_region
) {
	renderer_.CompositeRenderPassResult(color_source, destination, destination_region);
}

impl::FramebufferObject& DrawContext::GetPoolFramebuffer(impl::FramebufferId framebuffer) {
	return renderer_.framebuffer_pool_.GetFramebuffer(framebuffer);
}

void DrawContext::SetFramebuffer(impl::FramebufferObject* framebuffer) {
	renderer_.SetFramebuffer(framebuffer);
}

void DrawContext::SetViewport(Viewport viewport) {
	renderer_.SetViewport(viewport);
}

void DrawContext::SetScissor(const ScissorState& scissor) {
	renderer_.SetScissor(scissor);
}

} // namespace ptgn