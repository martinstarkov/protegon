#pragma once

#include "serialization/serialize.h"

namespace ptgn {

namespace editor {

struct EditorSettings {
	bool entity_picking{ true };
	bool render_only_selected_scene{ true };
	bool gizmo_uses_local_orientation{ false };
	bool show_read_only_inspector_data{ false };
	bool show_imgui_metrics{ false };

	PTGN_REFLECT(
		EditorSettings,
		entity_picking,
		render_only_selected_scene,
		gizmo_uses_local_orientation,
		show_read_only_inspector_data,
		show_imgui_metrics
	)
};

} // namespace editor

} // namespace ptgn
