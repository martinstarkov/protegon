#pragma once

#include "core/editor_context.h"
#include "core/math/angle.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"

namespace ptgn::editor {

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
	void DrawSelectedEntityGizmo(EditorContext& ctx, Viewport viewport);

	void DrawSceneCameraOutlines(EditorContext& ctx, Viewport image_viewport);

	EditorCamera editor_camera_;

	GizmoState gizmo_state_;

	bool use_editor_camera_{ true };
};

} // namespace ptgn::editor