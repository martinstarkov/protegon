#pragma once

#include "serialization/serialize.h"

namespace ptgn {

namespace editor {

struct EditorSettings {
	bool entity_picking{ true };
	bool gizmo_uses_local_orientation{ false };
	bool show_imgui_metrics{ false };

	PTGN_REFLECT(EditorSettings, entity_picking, gizmo_uses_local_orientation, show_imgui_metrics)
};

} // namespace editor

} // namespace ptgn