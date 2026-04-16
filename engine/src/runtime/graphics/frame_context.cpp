#include "runtime/graphics/frame_context.h"

#include <utility>

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scene/scene.h"

namespace ptgn {

FrameContext::FrameContext(const Scene& scene) :
	FrameContext{ scene.ctx().renderer, scene.render_target_, scene.ctx().camera } {}

FrameContext::FrameContext(
	const RenderContext& renderer, RenderTarget render_target_entity, Camera camera_entity
) {
	auto presentation_viewport{ renderer.GetPresentationViewport() };
	auto display_viewport{ renderer.GetDisplayViewport() };

	auto full_viewport_size{ renderer.GetFullViewportSize() };

	auto presentation_center{ presentation_viewport.position + presentation_viewport.size / 2.0f -
							  full_viewport_size / 2.0f };

	auto display_center_window{ display_viewport.position + display_viewport.size / 2.0f -
								presentation_viewport.size / 2.0f };

	presentation = PresentationFrame{ .presentation_center = presentation_center };

	display = DisplayFrame{ .display_center = display_center_window };

	render_target =
		RenderTargetFrame{ .render_target_transform = GetTransform(render_target_entity) };

	camera = CameraFrame{ .camera_viewport	  = camera_entity.GetViewport(),
						  .render_target_size = render_target_entity.GetSize(),
						  .scale			  = render_target_entity.GetScale() };

	world = WorldFrame{ .camera_transform = GetTransform(camera_entity) };
}

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
				p	 = WindowToPresentation(p, ctx.presentation);
				from = Presentation;
				break;
			case Presentation:
				p	 = PresentationToDisplay(p, ctx.display);
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
				p	 = DisplayToPresentation(p, ctx.display);
				from = Presentation;
				break;
			case Presentation:
				p	 = PresentationToWindow(p, ctx.presentation);
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

V2_float WindowToPresentation(V2_float window_point, const PresentationFrame& p) {
	return window_point - p.presentation_center;
}

V2_float PresentationToWindow(V2_float presentation_point, const PresentationFrame& p) {
	return presentation_point + p.presentation_center;
}

V2_float PresentationToDisplay(V2_float presentation_point, const DisplayFrame& display_frame) {
	return presentation_point - display_frame.display_center;
}

V2_float DisplayToPresentation(V2_float display_point, const DisplayFrame& display_frame) {
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