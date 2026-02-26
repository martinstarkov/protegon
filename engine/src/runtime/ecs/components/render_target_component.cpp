#include "runtime/ecs/components/render_target_component.h"

#include <utility>

#include "app/context.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

void RenderTargetGameResizeScript::OnEvent(EventDispatcher d) {
	d.Dispatch<GameResized>([this](auto& e) {
		auto& rt{ entity.Get<RenderTarget>() };
		// PTGN_LOG("Render target ", entity, " received game resize: ", e.size);
		rt.Resize(e.size);
	});
}

void RenderTargetDisplayResizeScript::OnEvent(EventDispatcher d) {
	d.Dispatch<DisplayResized>([this](auto& e) {
		auto& rt{ entity.Get<RenderTarget>() };
		// PTGN_LOG("Render target ", entity, " received display resize: ", e.size);
		rt.Resize(e.size);
	});
}

static Entity CreateRenderTarget(
	Entity render_target, Renderer& renderer, V2_int size, TextureFormat format
) {
	PTGN_ASSERT(render_target);

	SetPosition(render_target, {});

	// TODO: Fix.
	// SetDraw<RenderTarget>(render_target);
	Show(render_target);

	render_target.Add<RenderTarget>(renderer.CreateRenderTarget(size, format));
	// TODO: Add clear color here.
	render_target.Get<RenderTarget>().Clear();

	return render_target;
}

Entity CreateRenderTarget(
	Entity render_target, Renderer& renderer, ResizeMode resize_to_resolution,
	TextureFormat texture_format
) {
	PTGN_ASSERT(render_target);

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

	render_target = CreateRenderTarget(render_target, renderer, resolution, texture_format);

	PTGN_ASSERT(render_target);

	return render_target;
}

} // namespace impl

Entity CreateRenderTarget(
	Scene& scene, Renderer& renderer, ResizeMode resize_to_resolution, TextureFormat texture_format
) {
	return impl::CreateRenderTarget(
		scene.CreateEntity(), renderer, resize_to_resolution, texture_format
	);
}

Entity CreateRenderTarget(
	Scene& scene, Renderer& renderer, V2_int size, TextureFormat texture_format
) {
	return impl::CreateRenderTarget(scene.CreateEntity(), renderer, size, texture_format);
}

} // namespace ptgn