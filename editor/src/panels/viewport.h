#pragma once

#include "core/math/angle.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/graphics/frame_context.h"

namespace ptgn::editor {

class EditorContext;

enum class GizmoTool {
	None,
	Translate,
	Rotate,
	Scale
};

enum class GizmoHandle {
	None,
	MoveCenter,
	MoveX,
	MoveY,
	Rotate,
	ScaleX,
	ScaleY,
	ScaleUniform
};

struct GizmoState {
	GizmoTool tool{ GizmoTool::Translate };
	GizmoHandle hot{ GizmoHandle::None };
	GizmoHandle active{ GizmoHandle::None };

	V2_float drag_start_mouse_screen{};
	V2_float drag_start_mouse_world{};
	V2_float drag_start_position{};
	V2_float drag_start_scale{};
	Radians drag_start_rotation{};
	V2_float drag_start_pivot_screen{};
	V2_float drag_start_axis_x_world{};
	V2_float drag_start_axis_y_world{};
	V2_float drag_start_axis_x_screen{};
	V2_float drag_start_axis_y_screen{};

	V2_float pivot_world{};
};

struct EditorCamera {
	Camera camera;
	bool pixel_rounding{ true };
};

class ViewportPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	void DrawSelectedEntityGizmo(
		EditorContext& ctx, Viewport presentation_viewport, const FrameContext& frame_context
	);

	void DrawSceneCameraOutlines(
		EditorContext& ctx, Viewport presentation_viewport, const FrameContext& frame_context
	);

	void DrawViewportToolbar(EditorContext& ctx);

	void HandleEntityPicking(
		EditorContext& ctx, Viewport image_viewport, V2_int presentation_framebuffer_size,
		Viewport presentation_viewport, const FrameContext& frame_context
	);

	EditorCamera editor_camera_;

	GizmoState gizmo_state_;

	bool use_editor_camera_{ true };
};

} // namespace ptgn::editor