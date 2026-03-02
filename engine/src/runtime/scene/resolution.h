#pragma once

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/viewport.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/render_target_component.h"

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

struct CameraFrame {
	// Pixels in display frame of reference.
	Viewport camera_viewport;
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
	Camera,
	World
};

class FrameContext {
public:
	FrameContext() = default;

	FrameContext(const ApplicationContext& app, RenderTarget render_target, Camera camera);
	explicit FrameContext(const Scene& scene);

	DisplayFrame display;
	RenderTargetFrame render_target;
	CameraFrame camera;
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

[[nodiscard]] V2_float RenderTargetToCamera(V2_float render_target_point, CameraFrame camera_frame);
[[nodiscard]] V2_float CameraToRenderTarget(V2_float camera_point, CameraFrame camera_frame);

[[nodiscard]] V2_float CameraToWorld(V2_float camera_point, WorldFrame world_frame);
[[nodiscard]] V2_float WorldToCamera(V2_float world_point, WorldFrame world_frame);

} // namespace ptgn