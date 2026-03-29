#include "runtime/graphics/render_target_component.h"

#include <optional>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/texture_format.h"
#include "renderer/renderer.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

void RenderTargetGameResizeScript::OnEvent(EventDispatcher d) {
	d.Dispatch<InternalGameResized>([this](auto& e) {
		auto& rt{ entity.Get<RenderTargetObject>() };
		// PTGN_LOG("Render target ", entity, " received game resize: ", e.size);
		rt.Resize(e.size);
	});
}

void RenderTargetDisplayResizeScript::OnEvent(EventDispatcher d) {
	d.Dispatch<InternalDisplayResized>([this](auto& e) {
		auto& rt{ entity.Get<RenderTargetObject>() };
		// PTGN_LOG("Render target ", entity, " received display resize: ", e.size);
		rt.Resize(e.size);
	});
}

} // namespace impl

RenderTarget::RenderTarget(Entity entity) : Entity{ entity } {}

void RenderTarget::Bind() {
	Get<impl::RenderTargetObject>().Bind();
}

void RenderTarget::Clear(std::optional<Color> color, bool set_viewport) {
	Color clear{ color.value_or(GetOrDefault<ClearColor>().value) };
	Get<impl::RenderTargetObject>().Clear(clear, set_viewport);
}

void RenderTarget::SetClearColor(Color clear_color) {
	if (clear_color == ClearColor{}.value) {
		Remove<ClearColor>();
	} else {
		Add<ClearColor>(clear_color);
	}
}

Color RenderTarget::GetClearColor() const {
	return GetOrDefault<ClearColor>().value;
}

V2_float RenderTarget::GetScale() const {
	const auto& renderer{ GetScene().ctx().renderer };
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

void RenderTarget::Draw(DrawContext& renderer, Entity entity, Camera) {
	PTGN_ASSERT(entity.Has<impl::RenderTargetObject>());

	std::optional<V2_int> size;

	if (entity.Has<impl::TextureSize>()) {
		size = V2_int{ entity.Get<impl::TextureSize>() };
	} else {
		size = entity.Get<impl::RenderTargetObject>().GetSize();
	}

	PTGN_ASSERT(size.has_value(), "Render target does not have a texture");
	PTGN_ASSERT(!(*size).IsZero(), "Render target texture does not have a valid size");

	auto blend_mode{ GetBlendMode(entity) };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto transform{ GetDrawTransform(entity) };
	Rect rect{ V2_float{ *size } };
	auto positions{ rect.GetWorldVertices(transform, draw_origin) };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto texture_coordinates{ GetTextureCoordinates(entity, false) };
	auto texture{ entity.Get<impl::RenderTargetObject>().GetTextureId() };

	constexpr bool floor_positions{ true };

	renderer.SetBlend(blend_mode);
	renderer.DrawTexture(
		texture, positions, tint, depth.GetValue(), texture_coordinates, floor_positions
	);
}

void RenderTarget::AddRenderTargetComponents(
	RenderTarget render_target, SceneContext& ctx, V2_int size, Color clear_color,
	TextureFormat format
) {
	PTGN_ASSERT(render_target, "Failed to create render target entity");

	SetDraw<RenderTarget>(render_target);
	Show(render_target, false);
	render_target.SetClearColor(clear_color);

	render_target.Add<impl::RenderTargetObject>(
		ctx.global_renderer_.CreateRenderTarget(size, format)
	);
	render_target.Get<impl::RenderTargetObject>().Clear(clear_color, true);
}

void RenderTarget::AddRenderTargetComponents(
	RenderTarget render_target, SceneContext& ctx, ResizeMode resize_to_resolution,
	Color clear_color, TextureFormat texture_format
) {
	PTGN_ASSERT(render_target, "Failed to create render target entity");

	V2_int resolution;

	if (resize_to_resolution == ResizeMode::DisplaySize) {
		resolution = ctx.global_renderer_.GetDisplaySize();
		AddScript<impl::RenderTargetDisplayResizeScript>(render_target);
	} else if (resize_to_resolution == ResizeMode::GameSize) {
		resolution = ctx.global_renderer_.GetGameSize();
		AddScript<impl::RenderTargetGameResizeScript>(render_target);
	} else {
		PTGN_ERROR("Unknown resize to resolution value");
	}

	PTGN_ASSERT(
		resolution.BothAboveZero(), "Cannot create render target with an invalid resolution"
	);

	AddRenderTargetComponents(render_target, ctx, resolution, clear_color, texture_format);

	PTGN_ASSERT(render_target);
}

RenderTarget CreateRenderTarget(
	Scene& scene, ResizeMode resize_to_resolution, Color clear_color, TextureFormat texture_format
) {
	RenderTarget render_target{ scene.CreateEntity() };
	RenderTarget::AddRenderTargetComponents(
		render_target, scene.ctx(), resize_to_resolution, clear_color, texture_format
	);
	return render_target;
}

RenderTarget CreateRenderTarget(
	Scene& scene, V2_int size, Color clear_color, TextureFormat texture_format
) {
	RenderTarget render_target{ scene.CreateEntity() };
	RenderTarget::AddRenderTargetComponents(
		render_target, scene.ctx(), size, clear_color, texture_format
	);
	return render_target;
}

} // namespace ptgn