#include "runtime/graphics/render_target.h"

#include <algorithm>
#include <optional>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/render_state.h"
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

namespace ptgn {

namespace {

V2_int GetInitialDisplaySize(Renderer& renderer) {
	auto size{ renderer.GetDisplaySize() };

	if (!size.IsPositive()) {
		size = renderer.GetPresentationSize();
	}

	PTGN_ASSERT(
		size.IsPositive(), "Renderer display and presentation sizes cannot both be zero or negative"
	);

	return size;
}

RenderTarget CreateRenderTargetImpl(
	Scene& scene, Transform transform, impl::RenderTargetDesc target, Color clear_color
) {
	RenderTarget render_target{ scene.CreateEntity() };

	target.size = Max(target.size, V2_int{ 1, 1 });

	auto& renderer{ scene.ctx().renderer };

	const V2_int framebuffer_size{ target.follow_display_size ? GetInitialDisplaySize(renderer)
															  : target.size };

	render_target.Add<Tag>("Render Target");
	render_target.Add<Visible>(true);
	render_target.Add<Transform>(transform);
	render_target.Add<impl::RenderTargetDesc>(target);

	SetDraw<RenderTarget>(render_target);

	render_target.SetClearColor(clear_color);

	render_target.Add<impl::FramebufferObject>(impl::RendererAccessor{ renderer }.CreateFramebuffer(
		{
			.size	= framebuffer_size,
			.format = target.format,
		},
		std::nullopt
	));

	render_target.ClearColor(std::nullopt);

	return render_target;
}

} // namespace

RenderTarget::RenderTarget(Entity entity) : Entity{ entity } {}

void RenderTarget::Bind() {
	Get<impl::FramebufferObject>().Bind();
}

void RenderTarget::ClearColor(std::optional<Color> color) {
	if (color.has_value()) {
		Get<impl::FramebufferObject>().Clear(color.value());
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearColor>().color);
}

void RenderTarget::ClearDepth(std::optional<Depth> depth) {
	if (depth.has_value()) {
		Get<impl::FramebufferObject>().Clear(depth.value());
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearDepth>().depth);
}

void RenderTarget::ClearStencil(std::optional<Stencil> stencil) {
	if (stencil.has_value()) {
		Get<impl::FramebufferObject>().Clear(stencil.value());
		return;
	}

	Get<impl::FramebufferObject>().Clear(GetOrDefault<impl::ClearStencil>().stencil);
}

void RenderTarget::ClearDepthStencil(std::optional<DepthStencil> depth_stencil) {
	if (depth_stencil.has_value()) {
		Get<impl::FramebufferObject>().Clear(depth_stencil.value());
		return;
	}

	Get<impl::FramebufferObject>().Clear(
		GetClearDepthStencil().value_or(DepthStencil{})
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

	bool has_clear_value{ false };

	if (auto clear_depth{ TryGet<impl::ClearDepth>() }) {
		clear.depth		= clear_depth->depth;
		has_clear_value = true;
	}

	if (auto clear_stencil{ TryGet<impl::ClearStencil>() }) {
		clear.stencil	= clear_stencil->stencil;
		has_clear_value = true;
	}

	if (has_clear_value) {
		return clear;
	}

	return std::nullopt;
}

RenderTarget& RenderTarget::SetFollowDisplaySize(bool follow_display_size) {
	auto& target{ Get<impl::RenderTargetDesc>() };

	if (target.follow_display_size == follow_display_size) {
		return *this;
	}

	auto& renderer{ GetScene().ctx().renderer };

	if (!follow_display_size) {
		// Preserve the target's logical/world size when switching
		// from display following to a custom size.
		target.size = Max(renderer.GetLogicalSize(), V2_int{ 1, 1 });
	}

	target.follow_display_size = follow_display_size;

	UpdateSize(GetInitialDisplaySize(renderer));

	return *this;
}

RenderTarget& RenderTarget::SetSize(V2_int size) {
	PTGN_ASSERT(size.IsPositive(), "Render target size cannot be zero or negative");

	auto& target{ Get<impl::RenderTargetDesc>() };
	target.follow_display_size = false;
	target.size				   = size;

	UpdateSize(GetScene().ctx().renderer.GetDisplaySize());

	return *this;
}

bool RenderTarget::FollowsDisplaySize() const {
	if (auto target{ TryGet<impl::RenderTargetDesc>() }) {
		return target->follow_display_size;
	}

	return false;
}

V2_float RenderTarget::GetScale() const {
	V2_float logical_size{ GetScene().ctx().renderer.GetLogicalSize() };
	PTGN_ASSERT(logical_size.IsPositive(), "Logical size cannot be negative or zero");

	V2_float render_target_size{ GetSize() };
	V2_float scale{ render_target_size / logical_size };

	PTGN_ASSERT(scale.IsPositive(), "Render target scale cannot be negative or zero");

	return scale;
}

V2_int RenderTarget::GetCustomSize() const {
	if (auto target{ TryGet<impl::RenderTargetDesc>() }) {
		return target->size;
	}

	return GetSize();
}

V2_float RenderTarget::GetDrawSize() const {
	if (auto texture_size{ TryGet<impl::TextureSize>() }) {
		return *texture_size;
	}

	if (FollowsDisplaySize()) {
		return V2_float{ GetScene().ctx().renderer.GetLogicalSize() };
	}

	return V2_float{ GetCustomSize() };
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

bool RenderTarget::UpdateSize(V2_int display_size) {
	if (!Has<impl::RenderTargetDesc>()) {
		return false;
	}

	auto& target{ Get<impl::RenderTargetDesc>() };

	V2_int desired_size;

	if (target.follow_display_size) {
		if (!display_size.IsPositive()) {
			return false;
		}

		desired_size = display_size;
	} else {
		target.size = Max(target.size, V2_int{ 1, 1 });

		desired_size = target.size;
	}

	const bool recreate_framebuffer{ !Has<impl::FramebufferObject>() ||
									 Get<impl::FramebufferObject>().GetDesc().format !=
										 target.format };

	if (recreate_framebuffer) {
		auto& scene{ GetScene() };

		impl::RendererAccessor renderer{ scene.ctx().renderer };

		Add<impl::FramebufferObject>(renderer.CreateFramebuffer(
			{
				.size	= desired_size,
				.format = target.format,
			},
			std::nullopt
		));

		return true;
	}

	auto& framebuffer{ Get<impl::FramebufferObject>() };

	if (framebuffer.GetDesc().size == desired_size) {
		return false;
	}

	framebuffer.Resize(desired_size);
	return true;
}

impl::TextureId RenderTarget::GetTexture() const {
	return Get<impl::FramebufferObject>().GetTexture();
}

void RenderTarget::Draw(DrawContext& ctx, Entity entity) {
	if (!entity.Has<impl::FramebufferObject>()) {
		PTGN_WARN("Render target cannot be drawn without FramebufferObject component");
		return;
	}

	RenderTarget render_target{ entity };

	V2_float draw_size{ render_target.GetDrawSize() };

	if (!draw_size.IsPositive()) {
		PTGN_WARN("Render target draw size cannot be zero or negative");
		return;
	}

	auto draw_transform{ GetDrawTransform(entity) };

	auto texture{ render_target.GetTexture() };

	auto blend_mode{ GetBlendMode(entity) };

	auto params{ impl::GetTextureDrawParams(entity, draw_size, true, color::White) };

	ctx.SetBlendMode(blend_mode);
	ctx.DrawTexture(draw_transform, texture, std::move(params));
}

RenderTarget CreateRenderTarget(
	Scene& scene, Transform transform, V2_int size, Color clear_color, TextureFormat texture_format
) {
	PTGN_ASSERT(!size.HasNegative(), "Render target size cannot be negative");

	const bool follow_display_size{ size.IsZero() };

	if (follow_display_size) {
		size = scene.ctx().renderer.GetLogicalSize();

		PTGN_ASSERT(size.IsPositive(), "Renderer logical size cannot be zero or negative");
	}

	return CreateRenderTargetImpl(
		scene, transform,
		impl::RenderTargetDesc{
			.follow_display_size = follow_display_size, .size = size, .format = texture_format },
		clear_color
	);
}

RenderTarget CreateRenderTarget(
	Scene& scene, Transform transform, Color clear_color, TextureFormat texture_format
) {
	const V2_int logical_size{ scene.ctx().renderer.GetLogicalSize() };

	PTGN_ASSERT(logical_size.IsPositive(), "Renderer logical size cannot be zero or negative");

	return CreateRenderTargetImpl(
		scene, transform,
		impl::RenderTargetDesc{
			.follow_display_size = true, .size = logical_size, .format = texture_format },
		clear_color
	);
}

} // namespace ptgn