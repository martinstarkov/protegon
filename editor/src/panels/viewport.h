#pragma once

#include <cstdint>
#include <optional>

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

struct GizmoOccurrenceId {
	std::uint64_t value{ 0 };

	constexpr bool operator==(const GizmoOccurrenceId&) const = default;
};

struct GizmoState {
	GizmoTool tool{ GizmoTool::Translate };

	GizmoHandle hot{ GizmoHandle::None };
	std::optional<GizmoOccurrenceId> hot_occurrence;

	GizmoHandle active{ GizmoHandle::None };
	std::optional<GizmoOccurrenceId> active_occurrence;

	V2_float drag_start_mouse_screen;
	V2_float drag_start_mouse_world;
	V2_float drag_start_position;
	V2_float drag_start_scale;
	Radians drag_start_rotation;
	V2_float drag_start_pivot_screen;
	V2_float drag_start_axis_x_world;
	V2_float drag_start_axis_y_world;
	V2_float drag_start_axis_x_screen;
	V2_float drag_start_axis_y_screen;

	V2_float pivot_world;
};

struct EditorCamera {
	Camera camera;
	bool pixel_rounding{ true };
};

class ViewportPanel {
public:
	void OnRender(EditorContext& ctx);

	void SetUseEditorCamera(bool use_editor_camera);

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
	std::optional<std::uint64_t> gizmo_entity_uuid_;

	bool use_editor_camera_{ true };
	std::optional<V2_float> previous_aspect_locked_content_size_;
};

} // namespace ptgn::editor