#include "runtime/graphics/render_target.h"

#include <optional>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/pipeline/viewport_event.h"
#include "renderer/renderer.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

void RenderTargetResizeScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::DisplayResized>([this](const auto& resized) {
		if (resized.size.IsZero()) {
			return;
		}
		// PTGN_LOG("Render target ", entity, " received resize: ", resized.size);
		entity.Get<FramebufferObject>().Resize(resized.size);
	});
}

} // namespace impl

RenderTarget::RenderTarget(Entity entity) : Entity{ entity } {}

void RenderTarget::Bind() {
	Get<impl::FramebufferObject>().Bind();
}

void RenderTarget::ClearColor(std::optional<Color> color, bool restore_bind) {
	if (color.has_value()) {
		Get<impl::FramebufferObject>().Clear(color.value(), restore_bind);
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearColor>().color, restore_bind);
}

void RenderTarget::ClearDepth(std::optional<Depth> depth, bool restore_bind) {
	if (depth.has_value()) {
		Get<impl::FramebufferObject>().Clear(depth.value(), restore_bind);
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearDepth>().depth, restore_bind);
}

void RenderTarget::ClearStencil(std::optional<Stencil> stencil, bool restore_bind) {
	if (stencil.has_value()) {
		Get<impl::FramebufferObject>().Clear(stencil.value(), restore_bind);
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearStencil>().stencil, restore_bind);
}

void RenderTarget::ClearDepthStencil(std::optional<DepthStencil> depth_stencil, bool restore_bind) {
	if (depth_stencil.has_value()) {
		Get<impl::FramebufferObject>().Clear(depth_stencil.value(), restore_bind);
		return;
	}

	Get<impl::FramebufferObject>().Clear(
		GetClearDepthStencil().value_or(DepthStencil{}), restore_bind
	);
}

void RenderTarget::SetClearColor(Color clear_color) {
	Add<impl::ClearColor>(clear_color);
}

std::optional<Color> RenderTarget::GetClearColor() const {
	if (auto clear{ TryGet<impl::ClearColor>() }) {
		return clear->color;
	}
	return std::nullopt;
}

void RenderTarget::SetClearDepth(Depth clear_depth) {
	Add<impl::ClearDepth>(clear_depth);
}

std::optional<Depth> RenderTarget::GetClearDepth() const {
	if (auto clear{ TryGet<impl::ClearDepth>() }) {
		return clear->depth;
	}
	return std::nullopt;
}

void RenderTarget::SetClearStencil(Stencil clear_stencil) {
	Add<impl::ClearStencil>(clear_stencil);
}

std::optional<Stencil> RenderTarget::GetClearStencil() const {
	if (auto clear{ TryGet<impl::ClearStencil>() }) {
		return clear->stencil;
	}
	return std::nullopt;
}

void RenderTarget::SetClearDepthStencil(DepthStencil clear_depth_stencil) {
	SetClearDepth(clear_depth_stencil.depth);
	SetClearStencil(clear_depth_stencil.stencil);
}

std::optional<DepthStencil> RenderTarget::GetClearDepthStencil() const {
	DepthStencil clear;

	bool has_clear_depth{ false };

	if (auto clear_depth{ TryGet<impl::ClearDepth>() }) {
		clear.depth		= clear_depth->depth;
		has_clear_depth = true;
	}

	if (auto clear_stencil{ TryGet<impl::ClearStencil>() }) {
		clear.stencil	= clear_stencil->stencil;
		has_clear_depth = true;
	}

	if (has_clear_depth) {
		return clear;
	}

	return std::nullopt;
}

V2_float RenderTarget::GetScale() const {
	V2_float logical_size{ GetScene().ctx().renderer.GetLogicalSize() };
	PTGN_ASSERT(logical_size.IsPositive(), "Logical size cannot be negative or zero");
	V2_float rt_size{ GetSize() };
	V2_float scale{ rt_size / logical_size };
	PTGN_ASSERT(scale.IsPositive(), "Render target scale cannot be negative or zero");
	return scale;
}

V2_int RenderTarget::GetSize() const {
	return GetDesc().size;
}

TextureFormat RenderTarget::GetFormat() const {
	return GetDesc().format;
}

TextureParams RenderTarget::GetParams() const {
	return GetDesc().params;
}

TextureDesc RenderTarget::GetDesc() const {
	return Get<impl::FramebufferObject>().GetDesc();
}

impl::TextureId RenderTarget::GetTexture() const {
	return Get<impl::FramebufferObject>().GetTexture();
}

void RenderTarget::Draw(DrawContext& ctx, Entity entity) {
	PTGN_ASSERT(entity.Has<impl::FramebufferObject>());

	RenderTarget render_target{ entity };

	V2_int size;

	if (entity.Has<impl::TextureSize>()) {
		size = V2_float{ entity.Get<impl::TextureSize>() };
	} else {
		size = render_target.GetSize();
	}

	PTGN_ASSERT(size.IsPositive(), "Render target size cannot be zero or negative");

	auto draw_transform{ GetDrawTransform(entity) };
	auto texture{ render_target.GetTexture() };
	auto blend_mode{ GetBlendMode(entity) };

	auto params{ impl::GetTextureDrawParams(entity, size, true, color::White) };

	ctx.SetBlendMode(blend_mode);
	ctx.DrawTexture(draw_transform, texture, params);
}

RenderTarget CreateRenderTarget(
	Scene& scene, Transform transform, V2_int size, Color clear_color, TextureFormat texture_format
) {
	RenderTarget render_target{ scene.CreateEntity() };
	PTGN_DEFAULT_NAME(render_target, "Render Target");

	PTGN_ASSERT(!size.IsNegative(), "Render target size cannot be negative");

	if (size.IsZero()) {
		size = scene.ctx().renderer.GetPresentationViewport().size;
		AddScript<impl::RenderTargetResizeScript>(render_target);
	}

	PTGN_ASSERT(size.IsPositive(), "Render target size cannot be zero or negative");

	SetDraw<RenderTarget>(render_target);
	render_target.Add<impl::Visible>(true);
	SetTransform(render_target, transform);

	render_target.SetClearColor(clear_color);

	render_target.Add<impl::FramebufferObject>(
		impl::RendererAccessor{ scene.ctx().renderer }.CreateFramebuffer(
			{ .size{ size }, .format{ texture_format } }, std::nullopt
		)
	);

	render_target.ClearColor(std::nullopt, true);

	return render_target;
}

RenderTarget CreateRenderTarget(
	Scene& scene, Transform transform, Color clear_color, TextureFormat texture_format
) {
	return CreateRenderTarget(scene, transform, {}, clear_color, texture_format);
}

} // namespace ptgn