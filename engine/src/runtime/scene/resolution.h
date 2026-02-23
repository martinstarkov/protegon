#pragma once

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class ApplicationContext;
class Scene;

struct DisplayFrame {
	// Pixels relative to window center in window frame of reference.
	V2_float display_center;
};

struct RenderTargetFrame {
	// Position in pixels relative to display center in display frame of reference.
	Transform render_target_transform;
};

struct CameraViewportFrame {
	// Pixels in display frame of reference.
	V2_float render_target_size;
	// NDC offset in range: [-1.0, 1.0] where (0, 0) is the render target center.
	V2_float camera_viewport_center;
};

struct CameraViewFrame {
	// How many world units correspond to a pixel in render target frame of reference.
	V2_float world_units_per_render_target_pixel{ 1.0f };
};

struct WorldFrame {
	// Position in world units Relative to world center in world frame of reference.
	Transform camera_transform;
};

// Must be ordered from lowest rank to highest rank, where higher rank frames depend on lower rank
// frames of reference for their definition.
enum class Frame {
	Window,
	Display,
	RenderTarget,
	CameraViewport,
	CameraView,
	World
};

class FrameContext {
public:
	FrameContext() = default;

	FrameContext(
		const ApplicationContext& app, Entity render_target, Entity camera,
		V2_float world_units_per_render_target_pixel = V2_float{ 1.0 }
	);
	explicit FrameContext(Scene& scene);

	DisplayFrame display;
	RenderTargetFrame render_target;
	CameraViewportFrame camera_viewport;
	CameraViewFrame camera_view;
	WorldFrame world;
};

V2_float ConvertPoint(V2_float position, Frame from, Frame to, const FrameContext& ctx);

V2_float CenterToTopLeft(V2_float point_center, V2_float size);
V2_float TopLeftToCenter(V2_float point_top_left, V2_float size);

[[nodiscard]] V2_float WindowToDisplay(V2_float window_point, DisplayFrame display_frame);
[[nodiscard]] V2_float DisplayToWindow(V2_float display_point, DisplayFrame display_frame);

[[nodiscard]] V2_float DisplayToRenderTarget(
	V2_float display_point, RenderTargetFrame render_target_frame
);
[[nodiscard]] V2_float RenderTargetToDisplay(
	V2_float render_target_point, RenderTargetFrame render_target_frame
);

[[nodiscard]] V2_float RenderTargetToCameraViewport(
	V2_float render_target_point, CameraViewportFrame camera_viewport_frame
);
[[nodiscard]] V2_float CameraViewportToRenderTarget(
	V2_float camera_viewport_point, CameraViewportFrame camera_viewport_frame
);

[[nodiscard]] V2_float CameraViewportToCameraView(
	V2_float camera_viewport_point, CameraViewFrame camera_view_frame
);
[[nodiscard]] V2_float CameraViewToCameraViewport(
	V2_float camera_view_point, CameraViewFrame camera_view_frame
);

[[nodiscard]] V2_float CameraViewToWorld(V2_float camera_view_point, WorldFrame world_frame);
[[nodiscard]] V2_float WorldToCameraView(V2_float world_point, WorldFrame world_frame);

} // namespace ptgn