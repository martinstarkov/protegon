#pragma once

#include "core/editor_context.h"
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

class ViewportPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	void DrawSelectedEntityGizmo(EditorContext& ctx, Viewport viewport);

	GizmoState gizmo_state_;
};

} // namespace ptgn::editor