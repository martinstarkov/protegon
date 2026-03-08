#include "runtime/graphics/render_target_component.h"

#include <optional>

#include "app/context.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/texture.h"
#include "renderer/renderer.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

void RenderTargetGameResizeScript::OnEvent(EventDispatcher d) {
	d.Dispatch<GameResized>([this](auto& e) {
		auto& rt{ entity.Get<RenderTargetObject>() };
		// PTGN_LOG("Render target ", entity, " received game resize: ", e.size);
		rt.Resize(e.size);
	});
}

void RenderTargetDisplayResizeScript::OnEvent(EventDispatcher d) {
	d.Dispatch<DisplayResized>([this](auto& e) {
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
	Color clear;

	if (color.has_value()) {
		clear = *color;
	} else {
		clear = GetOrDefault<ClearColor>().value;
	}

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

V2_int RenderTarget::GetSize() const {
	return Get<impl::RenderTargetObject>().GetSize();
}

TextureFormat RenderTarget::GetFormat() const {
	return Get<impl::RenderTargetObject>().GetFormat();
}

RenderTarget::operator impl::TextureId() const {
	return Get<impl::RenderTargetObject>();
}

void RenderTarget::Draw(Renderer& renderer, Entity entity) {
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
	auto positions{ Rect{ *size }.GetWorldVertices(transform, draw_origin) };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto texture_coordinates{ GetTextureCoordinates(entity, false) };
	auto texture{ entity.Get<impl::RenderTargetObject>().operator impl::TextureId() };

	renderer.SetBlend(blend_mode);
	renderer.DrawQuadTexture(
		texture, positions, tint, static_cast<float>(depth.GetValue()), false, texture_coordinates
	);
}

void RenderTarget::AddRenderTargetComponents(
	RenderTarget render_target, Renderer& renderer, V2_int size, Color clear_color,
	TextureFormat format
) {
	PTGN_ASSERT(render_target, "Failed to create render target entity");

	SetDraw<RenderTarget>(render_target);
	Show(render_target, false);
	render_target.SetClearColor(clear_color);

	render_target.Add<impl::RenderTargetObject>(renderer.CreateRenderTarget(size, format));
	render_target.Get<impl::RenderTargetObject>().Clear(clear_color, true);
}

void RenderTarget::AddRenderTargetComponents(
	RenderTarget render_target, Renderer& renderer, ResizeMode resize_to_resolution,
	Color clear_color, TextureFormat texture_format
) {
	PTGN_ASSERT(render_target, "Failed to create render target entity");

	V2_int resolution;

	if (resize_to_resolution == ResizeMode::DisplaySize) {
		resolution = renderer.GetDisplaySize();
		AddScript<impl::RenderTargetDisplayResizeScript>(render_target);
	} else if (resize_to_resolution == ResizeMode::GameSize) {
		resolution = renderer.GetGameSize();
		AddScript<impl::RenderTargetGameResizeScript>(render_target);
	} else {
		PTGN_ERROR("Unknown resize to resolution value");
	}

	PTGN_ASSERT(
		resolution.BothAboveZero(), "Cannot create render target with an invalid resolution"
	);

	AddRenderTargetComponents(render_target, renderer, resolution, clear_color, texture_format);

	PTGN_ASSERT(render_target);
}

RenderTarget CreateRenderTarget(
	Scene& scene, ResizeMode resize_to_resolution, Color clear_color, TextureFormat texture_format
) {
	RenderTarget render_target{ scene.CreateEntity() };
	RenderTarget::AddRenderTargetComponents(
		render_target, scene.app().renderer, resize_to_resolution, clear_color, texture_format
	);
	return render_target;
}

RenderTarget CreateRenderTarget(
	Scene& scene, V2_int size, Color clear_color, TextureFormat texture_format
) {
	RenderTarget render_target{ scene.CreateEntity() };
	RenderTarget::AddRenderTargetComponents(
		render_target, scene.app().renderer, size, clear_color, texture_format
	);
	return render_target;
}

} // namespace ptgn