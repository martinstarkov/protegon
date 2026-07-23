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

V2_int ClampRenderTargetSize(V2_int size) {
	return {
		std::max(size.x, 1),
		std::max(size.y, 1),
	};
}

V2_int GetInitialDisplaySize(Renderer& renderer) {
	auto size{ renderer.GetDisplaySize() };

	if (!size.IsPositive()) {
		size = renderer.GetPresentationSize();
	}

	PTGN_ASSERT(
		size.IsPositive(),
		"Renderer display and presentation sizes cannot both be zero or negative"
	);

	return size;
}

RenderTarget CreateRenderTargetImpl(
	Scene& scene,
	Transform transform,
	impl::RenderTargetSize target_size,
	Color clear_color,
	TextureFormat texture_format
) {
	RenderTarget render_target{ scene.CreateEntity() };

	target_size.size = ClampRenderTargetSize(target_size.size);

	render_target.Add<Tag>("Render Target");
	render_target.Add<Visible>(true);
	render_target.Add<Transform>(transform);
	render_target.Add<impl::RenderTargetSize>(target_size);

	SetDraw<RenderTarget>(render_target);

	render_target.SetClearColor(clear_color);

	render_target.Add<impl::FramebufferObject>(
		impl::RendererAccessor{ scene.ctx().renderer }.CreateFramebuffer(
			{
				.size = target_size.size,
				.format = texture_format,
			},
			std::nullopt
		)
	);

	render_target.ClearColor(std::nullopt, true);

	return render_target;
}

} // namespace

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

void RenderTarget::ClearDepthStencil(
	std::optional<DepthStencil> depth_stencil,
	bool restore_bind
) {
	if (depth_stencil.has_value()) {
		Get<impl::FramebufferObject>().Clear(depth_stencil.value(), restore_bind);
		return;
	}

	Get<impl::FramebufferObject>().Clear(
		GetClearDepthStencil().value_or(DepthStencil{}),
		restore_bind
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
		clear.depth = clear_depth->depth;
		has_clear_value = true;
	}

	if (auto clear_stencil{ TryGet<impl::ClearStencil>() }) {
		clear.stencil = clear_stencil->stencil;
		has_clear_value = true;
	}

	if (has_clear_value) {
		return clear;
	}

	return std::nullopt;
}

RenderTarget& RenderTarget::SetFollowDisplaySize(bool follow_display_size) {
	auto& target_size{ Get<impl::RenderTargetSize>() };

	if (target_size.follow_display_size == follow_display_size) {
		return *this;
	}

	if (!follow_display_size) {
		// Turning tracking off freezes the current actual size.
		target_size.size = GetSize();
	}

	target_size.follow_display_size = follow_display_size;

	UpdateSize(GetScene().ctx().renderer.GetDisplaySize());

	return *this;
}

RenderTarget& RenderTarget::SetSize(V2_int size) {
	PTGN_ASSERT(size.IsPositive(), "Render target size cannot be zero or negative");

	auto& target_size{ Get<impl::RenderTargetSize>() };
	target_size.follow_display_size = false;
	target_size.size = size;

	UpdateSize(GetScene().ctx().renderer.GetDisplaySize());

	return *this;
}

bool RenderTarget::FollowsDisplaySize() const {
	if (auto target_size{ TryGet<impl::RenderTargetSize>() }) {
		return target_size->follow_display_size;
	}

	return false;
}

V2_int RenderTarget::GetConfiguredSize() const {
	if (auto target_size{ TryGet<impl::RenderTargetSize>() }) {
		return target_size->size;
	}

	return GetSize();
}

V2_float RenderTarget::GetScale() const {
	V2_float logical_size{ GetScene().ctx().renderer.GetLogicalSize() };
	PTGN_ASSERT(logical_size.IsPositive(), "Logical size cannot be negative or zero");

	V2_float render_target_size{ GetSize() };
	V2_float scale{ render_target_size / logical_size };

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

bool RenderTarget::UpdateSize(V2_int display_size) {
	if (!Has<impl::FramebufferObject, impl::RenderTargetSize>()) {
		return false;
	}

	auto& target_size{ Get<impl::RenderTargetSize>() };

	if (target_size.follow_display_size) {
		if (!display_size.IsPositive()) {
			return false;
		}

		target_size.size = display_size;
	} else {
		target_size.size = ClampRenderTargetSize(target_size.size);
	}

	auto& framebuffer{ Get<impl::FramebufferObject>() };

	if (framebuffer.GetDesc().size == target_size.size) {
		return false;
	}

	framebuffer.Resize(target_size.size);

	return true;
}

impl::TextureId RenderTarget::GetTexture() const {
	return Get<impl::FramebufferObject>().GetTexture();
}

void RenderTarget::Draw(DrawContext& ctx, Entity entity) {
	if (!entity.Has<impl::FramebufferObject>()) {
		return;
	}

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
	Scene& scene,
	Transform transform,
	V2_int size,
	Color clear_color,
	TextureFormat texture_format
) {
	PTGN_ASSERT(!size.IsNegative(), "Render target size cannot be negative");

	bool follow_display_size{ size.IsZero() };

	if (follow_display_size) {
		size = GetInitialDisplaySize(scene.ctx().renderer);
	}

	return CreateRenderTargetImpl(
		scene,
		transform,
		impl::RenderTargetSize{
			.follow_display_size = follow_display_size,
			.size = size,
		},
		clear_color,
		texture_format
	);
}

RenderTarget CreateRenderTarget(
	Scene& scene,
	Transform transform,
	Color clear_color,
	TextureFormat texture_format
) {
	return CreateRenderTargetImpl(
		scene,
		transform,
		impl::RenderTargetSize{
			.follow_display_size = true,
			.size = GetInitialDisplaySize(scene.ctx().renderer),
		},
		clear_color,
		texture_format
	);
}

} // namespace ptgn