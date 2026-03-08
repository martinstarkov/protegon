#include "runtime/scene/resolution.h"

#include <memory>
#include <utility>

#include "app/context.h"
#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/viewport.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/render_target_component.h"
#include "runtime/scene/scene.h"

namespace ptgn {

FrameContext::FrameContext(const Scene& scene) :
	FrameContext{ *scene.ctx_, scene.render_target_, scene.camera } {}

FrameContext::FrameContext(
	const ApplicationContext& app, RenderTarget render_target_entity, Camera camera_entity
) :
	display{ app.renderer.GetDisplayViewport().position },
	render_target{ GetTransform(render_target_entity) },
	camera{ camera_entity.GetViewport(), render_target_entity.GetSize(), app.renderer.GetScale() },
	world{ GetTransform(camera_entity) } {}

V2_float ConvertPoint(V2_float p, Frame from, Frame to, const FrameContext& ctx) {
	int a{ std::to_underlying(from) };
	int b{ std::to_underlying(to) };
	if (a == b) {
		return p;
	}

	// Move "up" (towards World)
	while (a <= b) {
		switch (from) {
			using enum Frame;
			case Window:
				p	 = WindowToDisplay(p, ctx.display);
				from = Display;
				break;
			case Display:
				p	 = DisplayToRenderTarget(p, ctx.render_target);
				from = RenderTarget;
				break;
			case RenderTarget:
				p	 = RenderTargetToCamera(p, ctx.camera);
				from = Camera;
				break;
			case Camera:
				p	 = CameraToWorld(p, ctx.world);
				from = World;
				break;
			case World: break;
		}
		if (a == b) {
			return p;
		}
		++a;
	}

	// Move "down" (towards Window)
	while (a >= b) {
		switch (from) {
			using enum Frame;
			case World:
				p	 = WorldToCamera(p, ctx.world);
				from = Camera;
				break;
			case Camera:
				p	 = CameraToRenderTarget(p, ctx.camera);
				from = RenderTarget;
				break;
			case RenderTarget:
				p	 = RenderTargetToDisplay(p, ctx.render_target);
				from = Display;
				break;
			case Display:
				p	 = DisplayToWindow(p, ctx.display);
				from = Window;
				break;
			case Window: break;
		}
		if (a == b) {
			return p;
		}
		--a;
	}

	return p;
}

V2_float CenterToTopLeft(V2_float point_center, V2_float size) {
	PTGN_ASSERT(size.BothAboveZero());
	return point_center + size * 0.5f;
}

V2_float TopLeftToCenter(V2_float point_top_left, V2_float size) {
	PTGN_ASSERT(size.BothAboveZero());
	return point_top_left - size * 0.5f;
}

V2_float WindowToDisplay(V2_float window_point, const DisplayFrame& display_frame) {
	return window_point - display_frame.display_center;
}

V2_float DisplayToWindow(V2_float display_point, const DisplayFrame& display_frame) {
	return display_point + display_frame.display_center;
}

V2_float DisplayToRenderTarget(
	V2_float display_point, const RenderTargetFrame& render_target_frame
) {
	return render_target_frame.render_target_transform.ApplyInverse(display_point);
}

V2_float RenderTargetToDisplay(
	V2_float render_target_point, const RenderTargetFrame& render_target_frame
) {
	return render_target_frame.render_target_transform.Apply(render_target_point);
}

V2_float RenderTargetToCamera(V2_float render_target_point, const CameraFrame& camera_frame) {
	PTGN_ASSERT(camera_frame.scale.BothAboveZero(), "Display scale cannot be negative or zero");
	return (render_target_point + camera_frame.render_target_size / 2.0f) / camera_frame.scale -
		   (camera_frame.camera_viewport.position + camera_frame.camera_viewport.size / 2.0f);
}

V2_float CameraToRenderTarget(V2_float camera_point, const CameraFrame& camera_frame) {
	PTGN_ASSERT(camera_frame.scale.BothAboveZero(), "Display scale cannot be negative or zero");
	return (camera_point + camera_frame.camera_viewport.position +
			camera_frame.camera_viewport.size / 2.0f) *
			   camera_frame.scale -
		   camera_frame.render_target_size / 2.0f;
}

V2_float CameraToWorld(V2_float camera_point, const WorldFrame& world_frame) {
	return world_frame.camera_transform.Apply(camera_point);
}

V2_float WorldToCamera(V2_float world_point, const WorldFrame& world_frame) {
	return world_frame.camera_transform.ApplyInverse(world_point);
}

} // namespace ptgn