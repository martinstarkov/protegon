#include "runtime/graphics/frame_context.h"

#include <utility>

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

FrameContext::FrameContext(const Scene& scene) :
	FrameContext{ scene.ctx().renderer, scene.ctx().camera.GetRenderTarget(),
				  scene.ctx().camera.operator Camera() } {}

FrameContext::FrameContext(
	const Renderer& renderer, Transform render_target_transform, V2_float render_target_size,
	Transform camera_transform, Viewport camera_display_viewport
) {
	auto presentation_viewport{ renderer.GetPresentationViewport() };
	auto display_viewport{ renderer.GetDisplayViewport() };

	auto window_size{ renderer.GetWindowSize() };

	auto logical_size{ renderer.GetLogicalSize() };

	PTGN_ASSERT(logical_size.IsPositive(), "Logical size must be positive");
	PTGN_ASSERT(window_size.IsPositive(), "Window size must be positive");

	V2_int presentation_center{ presentation_viewport.GetCenter() };

	V2_int half_window{ window_size / 2.0f };

	V2_int display_center{ display_viewport.GetCenter() };

	V2_int half_presentation{ presentation_viewport.size / 2.0f };

	V2_int display_center_window{ display_center - half_presentation };

	presentation = PresentationFrame{ .presentation_center = presentation_center - half_window };

	display = DisplayFrame{ .display_center = display_center_window };

	render_target = RenderTargetFrame{ .render_target_transform = render_target_transform };

	camera = CameraFrame{ .camera_display_viewport = camera_display_viewport,
						  .render_target_size	   = render_target_size,
						  .scale				   = render_target_size / logical_size };

	world = WorldFrame{ .camera_transform = camera_transform };
}

FrameContext::FrameContext(
	const Renderer& renderer, RenderTarget render_target_entity, const Camera& cam
) :
	FrameContext{ renderer, GetTransform(render_target_entity),
				  render_target_entity == render_target_entity.GetScene().GetRenderTarget()
					  ? renderer.GetDisplayViewport().size
					  : V2_float{ render_target_entity.GetSize() },
				  cam.transform,
				  GetDisplayViewport(
					  cam.raw_viewport, cam.viewport_space, renderer.GetLogicalSize(),
					  render_target_entity == render_target_entity.GetScene().GetRenderTarget()
						  ? renderer.GetDisplayViewport().size
						  : V2_float{ render_target_entity.GetSize() }
				  ) } {}

V2_float ConvertPoint(V2_float p, Frame from, Frame to, const FrameContext& ctx) {
	int a{ std::to_underlying(from) };
	int b{ std::to_underlying(to) };

	while (a < b) {
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

		++a;
	}

	while (a > b) {
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

		--a;
	}

	return p;
}

V2_float CenterToTopLeft(V2_float point_center, V2_float size) {
	PTGN_ASSERT(size.IsPositive());
	return point_center + size * 0.5f;
}

V2_float TopLeftToCenter(V2_float point_top_left, V2_float size) {
	PTGN_ASSERT(size.IsPositive());
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
	PTGN_ASSERT(camera_frame.scale.IsPositive(), "Camera scale must be positive");

	auto render_target_top_left_point{
		CenterToTopLeft(render_target_point, camera_frame.render_target_size)
	};

	auto display_viewport_center{ camera_frame.camera_display_viewport.GetCenter() };

	auto scaled_camera_point{ render_target_top_left_point - display_viewport_center };

	auto camera_point{ scaled_camera_point / camera_frame.scale };

	return camera_point;
}

V2_float CameraToRenderTarget(V2_float camera_point, const CameraFrame& camera_frame) {
	PTGN_ASSERT(camera_frame.scale.IsPositive(), "Camera scale must be positive");

	auto display_viewport_center{ camera_frame.camera_display_viewport.GetCenter() };

	auto scaled_camera_point{ camera_point * camera_frame.scale };

	auto render_target_top_left_point{ scaled_camera_point + display_viewport_center };

	auto render_target_point{
		TopLeftToCenter(render_target_top_left_point, camera_frame.render_target_size)
	};

	return render_target_point;
}

V2_float CameraToWorld(V2_float camera_point, const WorldFrame& world_frame) {
	return world_frame.camera_transform.Apply(camera_point);
}

V2_float WorldToCamera(V2_float world_point, const WorldFrame& world_frame) {
	return world_frame.camera_transform.ApplyInverse(world_point);
}

} // namespace ptgn