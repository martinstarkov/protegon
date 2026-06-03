#include "runtime/graphics/render_target.h"

#include <optional>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport_event.h"
#include "renderer/renderer.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

void RenderTargetGameResizeScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::GameResized>([this](const auto& resized) {
		// PTGN_LOG("Render target ", entity, " received game resize: ", resized.size);
		entity.Get<FramebufferObject>().Resize(resized.size);
	});
}

void RenderTargetDisplayResizeScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::DisplayResized>([this](const auto& resized) {
		// PTGN_LOG("Render target ", entity, " received display resize: ", resized.size);
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
		Get<impl::FramebufferObject>().Clear(*color, restore_bind);
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearColor>().color, restore_bind);
}

void RenderTarget::ClearDepth(std::optional<Depth> depth, bool restore_bind) {
	if (depth.has_value()) {
		Get<impl::FramebufferObject>().Clear(*depth, restore_bind);
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearDepth>().depth, restore_bind);
}

void RenderTarget::ClearStencil(std::optional<Stencil> stencil, bool restore_bind) {
	if (stencil.has_value()) {
		Get<impl::FramebufferObject>().Clear(*stencil, restore_bind);
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearStencil>().stencil, restore_bind);
}

void RenderTarget::ClearDepthStencil(std::optional<DepthStencil> depth_stencil, bool restore_bind) {
	if (depth_stencil.has_value()) {
		Get<impl::FramebufferObject>().Clear(*depth_stencil, restore_bind);
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
	V2_float game_size{ GetScene().ctx().renderer.GetGameSize() };
	PTGN_ASSERT(game_size.IsPositive(), "Game size cannot be negative or zero");
	V2_float rt_size{ GetSize() };
	V2_float scale{ rt_size / game_size };
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

	std::optional<V2_int> size;

	if (entity.Has<impl::TextureSize>()) {
		size = V2_float{ entity.Get<impl::TextureSize>() };
	} else {
		size = render_target.GetSize();
	}

	PTGN_ASSERT(size.has_value(), "Render target does not have a texture");
	PTGN_ASSERT(!(*size).IsZero(), "Render target texture does not have a valid size");

	auto draw_transform{ GetDrawTransform(entity) };
	auto texture{ render_target.GetTexture() };
	auto blend_mode{ GetBlendMode(entity) };

	auto params{ impl::GetTextureDrawParams(entity, *size, true, color::White) };

	ctx.WithBlendMode(blend_mode, [&ctx, draw_transform, texture, &params]() {
		ctx.DrawTexture(draw_transform, texture, params);
	});
}

void RenderTarget::AddRenderTargetComponents(
	RenderTarget render_target, Scene& scene, V2_int size, Color clear_color, TextureFormat format
) {
	PTGN_ASSERT(render_target, "Failed to create render target entity");

	SetDraw<RenderTarget>(render_target);
	Show(render_target, false);

	render_target.SetClearColor(clear_color);

	render_target.Add<impl::FramebufferObject>(
		impl::RendererAccessor{ scene.ctx().renderer }.CreateFramebuffer(
			{ .size{ size }, .format{ format } }, std::nullopt
		)
	);
	render_target.ClearColor(std::nullopt, true);
}

void RenderTarget::AddRenderTargetComponents(
	RenderTarget render_target, Scene& scene, ResizeType resize_to_resolution, Color clear_color,
	TextureFormat texture_format
) {
	PTGN_ASSERT(render_target, "Failed to create render target entity");

	V2_int resolution;

	if (resize_to_resolution == ResizeType::Display) {
		resolution = scene.ctx().renderer.GetDisplaySize();
		AddScript<impl::RenderTargetDisplayResizeScript>(render_target);
	} else if (resize_to_resolution == ResizeType::Game) {
		resolution = scene.ctx().renderer.GetGameSize();
		AddScript<impl::RenderTargetGameResizeScript>(render_target);
	} else {
		PTGN_ERROR("Unknown resize to resolution value");
	}

	PTGN_ASSERT(resolution.IsPositive(), "Cannot create render target with an invalid resolution");

	AddRenderTargetComponents(render_target, scene, resolution, clear_color, texture_format);

	PTGN_ASSERT(render_target);
}

RenderTarget CreateRenderTarget(
	Scene& scene, ResizeType resize_to_resolution, Color clear_color, TextureFormat texture_format
) {
	RenderTarget render_target{ scene.CreateEntity() };
	RenderTarget::AddRenderTargetComponents(
		render_target, scene, resize_to_resolution, clear_color, texture_format
	);
	return render_target;
}

RenderTarget CreateRenderTarget(
	Scene& scene, V2_int size, Color clear_color, TextureFormat texture_format
) {
	RenderTarget render_target{ scene.CreateEntity() };
	RenderTarget::AddRenderTargetComponents(
		render_target, scene, size, clear_color, texture_format
	);
	return render_target;
}

} // namespace ptgn