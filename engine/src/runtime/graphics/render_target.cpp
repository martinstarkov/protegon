#include "runtime/graphics/render_target.h"

#include <optional>
#include <span>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport_event.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

void RenderTargetGameResizeScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::GameResized>([this](const auto& resized) {
		auto& rt{ entity.Get<RenderTargetObject>() };
		// PTGN_LOG("Render target ", entity, " received game resize: ", resized.size);
		rt.Resize(resized.size);
	});
}

void RenderTargetDisplayResizeScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::DisplayResized>([this](const auto& resized) {
		auto& rt{ entity.Get<RenderTargetObject>() };
		// PTGN_LOG("Render target ", entity, " received display resize: ", resized.size);
		rt.Resize(resized.size);
	});
}

ClearColor::operator ptgn::Color() const {
	return color;
}

} // namespace impl

RenderTarget::RenderTarget(Entity entity) : Entity{ entity } {}

void RenderTarget::Bind() {
	Get<impl::RenderTargetObject>().Bind();
}

void RenderTarget::Clear(std::optional<Color> color, bool set_viewport, bool restore_bind) {
	Color clear{ color.value_or(GetOrDefault<impl::ClearColor>()) };
	Get<impl::RenderTargetObject>().Clear(clear, set_viewport, restore_bind);
}

void RenderTarget::SetClearColor(Color clear_color) {
	if (clear_color == impl::ClearColor{}) {
		Remove<impl::ClearColor>();
	} else {
		Add<impl::ClearColor>(clear_color);
	}
}

Color RenderTarget::GetClearColor() const {
	return GetOrDefault<impl::ClearColor>();
}

V2_float RenderTarget::GetScale() const {
	const auto& renderer{ GetScene().ctx().global_renderer_ };
	V2_float game_size{ renderer.GetGameSize() };
	PTGN_ASSERT(game_size.BothAboveZero(), "Game size cannot be negative or zero");
	V2_float rt_size{ GetSize() };
	V2_float scale{ rt_size / game_size };
	PTGN_ASSERT(scale.BothAboveZero(), "Render target scale cannot be negative or zero");
	return scale;
}

V2_int RenderTarget::GetSize() const {
	return Get<impl::RenderTargetObject>().GetSize();
}

TextureFormat RenderTarget::GetFormat() const {
	return Get<impl::RenderTargetObject>().GetFormat();
}

RenderTarget::operator impl::RenderTargetId() const {
	return Get<impl::RenderTargetObject>().operator impl::RenderTargetId();
}

void RenderTarget::Draw(DrawContext& ctx, Entity entity) {
	PTGN_ASSERT(entity.Has<impl::RenderTargetObject>());

	std::optional<V2_int> size;

	if (entity.Has<impl::TextureSize>()) {
		size = V2_float{ entity.Get<impl::TextureSize>() };
	} else {
		size = entity.Get<impl::RenderTargetObject>().GetSize();
	}

	PTGN_ASSERT(size.has_value(), "Render target does not have a texture");
	PTGN_ASSERT(!(*size).IsZero(), "Render target texture does not have a valid size");

	auto blend_mode{ GetBlendMode(entity) };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto draw_transform{ GetDrawTransform(entity) };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto tex_coords{ GetTextureCoordinates(entity, false) };
	auto texture{ entity.Get<impl::RenderTargetObject>().GetTextureId() };
	auto entity_id{ entity.GetUUID() };

	auto effects{ impl::GetEffectParams(entity) };

	std::span<const impl::TextureBinding> extra_textures{};

	ctx.WithBlendMode(blend_mode, [&]() {
		ctx.DrawTexture(
			texture, draw_transform, depth, *size, draw_origin, tint, tex_coords, effects,
			extra_textures, entity_id
		);
	});
}

void RenderTarget::AddRenderTargetComponents(
	RenderTarget render_target, Scene& scene, V2_int size, Color clear_color, TextureFormat format
) {
	PTGN_ASSERT(render_target, "Failed to create render target entity");

	SetDraw<RenderTarget>(render_target);
	Show(render_target, false);
	render_target.SetClearColor(clear_color);

	render_target.Add<impl::RenderTargetObject>(
		scene.ctx().global_renderer_.CreateRenderTarget({ .size{ size }, .format{ format } })
	);
	render_target.Clear(clear_color, true, true);
}

void RenderTarget::AddRenderTargetComponents(
	RenderTarget render_target, Scene& scene, ResizeType resize_to_resolution, Color clear_color,
	TextureFormat texture_format
) {
	PTGN_ASSERT(render_target, "Failed to create render target entity");

	V2_int resolution;

	if (resize_to_resolution == ResizeType::Display) {
		resolution = scene.ctx().global_renderer_.GetDisplaySize();
		AddScript<impl::RenderTargetDisplayResizeScript>(render_target);
	} else if (resize_to_resolution == ResizeType::Game) {
		resolution = scene.ctx().global_renderer_.GetGameSize();
		AddScript<impl::RenderTargetGameResizeScript>(render_target);
	} else {
		PTGN_ERROR("Unknown resize to resolution value");
	}

	PTGN_ASSERT(
		resolution.BothAboveZero(), "Cannot create render target with an invalid resolution"
	);

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