#include "runtime/scene/resolution.h"

#include <memory>
#include <utility>

#include "app/context.h"
#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "renderer/resources/render_target.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn {

FrameContext::FrameContext(Scene& scene) :
	FrameContext{ *scene.ctx_, scene.render_target_, scene.camera,
				  scene.world_units_per_render_target_pixel_ } {}

FrameContext::FrameContext(
	const ApplicationContext& app, Entity render_target_entity, Entity camera,
	V2_float world_units_per_render_target_pixel
) :
	display{ app.renderer.GetDisplayViewport().GetCenter() },
	render_target{ GetTransform(render_target_entity) },
	camera_viewport{ render_target_entity.Get<RenderTarget>().GetSize(),
					 GetCameraViewport(camera).GetCenter() },
	camera_view{ world_units_per_render_target_pixel },
	world{ GetTransform(camera) } {}

V2_float ConvertPoint(V2_float p, Frame from, Frame to, const FrameContext& ctx) {
	int a{ std::to_underlying(from) };
	int b{ std::to_underlying(to) };
	if (a == b) {
		return p;
	}

	// Move "up" (towards World)
	while (a < b) {
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
				p	 = RenderTargetToCameraViewport(p, ctx.camera_viewport);
				from = CameraViewport;
				break;
			case CameraViewport:
				p	 = CameraViewportToCameraView(p, ctx.camera_view);
				from = CameraView;
				break;
			case CameraView:
				p	 = CameraViewToWorld(p, ctx.world);
				from = World;
				break;
			case World: break;
		}
		++a;
	}

	// Move "down" (towards Window)
	while (a > b) {
		switch (from) {
			using enum Frame;
			case World:
				p	 = WorldToCameraView(p, ctx.world);
				from = CameraView;
				break;
			case CameraView:
				p	 = CameraViewToCameraViewport(p, ctx.camera_view);
				from = CameraViewport;
				break;
			case CameraViewport:
				p	 = CameraViewportToRenderTarget(p, ctx.camera_viewport);
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

V2_float WindowToDisplay(V2_float window_point, DisplayFrame display_frame) {
	return window_point - display_frame.display_center;
}

V2_float DisplayToWindow(V2_float display_point, DisplayFrame display_frame) {
	return display_point + display_frame.display_center;
}

V2_float DisplayToRenderTarget(V2_float display_point, RenderTargetFrame render_target_frame) {
	return render_target_frame.render_target_transform.ApplyInverse(display_point);
}

V2_float RenderTargetToDisplay(
	V2_float render_target_point, RenderTargetFrame render_target_frame
) {
	return render_target_frame.render_target_transform.Apply(render_target_point);
}

V2_float RenderTargetToCameraViewport(
	V2_float render_target_point, CameraViewportFrame camera_viewport_frame
) {
	return render_target_point -
		   camera_viewport_frame.render_target_size * camera_viewport_frame.camera_viewport_center;
}

V2_float CameraViewportToRenderTarget(
	V2_float camera_viewport_point, CameraViewportFrame camera_viewport_frame
) {
	return camera_viewport_point +
		   camera_viewport_frame.render_target_size * camera_viewport_frame.camera_viewport_center;
}

V2_float CameraViewportToCameraView(
	V2_float camera_viewport_point, CameraViewFrame camera_view_frame
) {
	PTGN_ASSERT(camera_view_frame.world_units_per_render_target_pixel.BothAboveZero());
	return camera_viewport_point * camera_view_frame.world_units_per_render_target_pixel;
}

V2_float CameraViewToCameraViewport(V2_float camera_view_point, CameraViewFrame camera_view_frame) {
	PTGN_ASSERT(camera_view_frame.world_units_per_render_target_pixel.BothAboveZero());
	return camera_view_point / camera_view_frame.world_units_per_render_target_pixel;
}

V2_float CameraViewToWorld(V2_float camera_view_point, WorldFrame world_frame) {
	return world_frame.camera_transform.Apply(camera_view_point);
}

V2_float WorldToCameraView(V2_float world_point, WorldFrame world_frame) {
	return world_frame.camera_transform.ApplyInverse(world_point);
}

} // namespace ptgn