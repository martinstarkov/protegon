#pragma once

#include "serialization/serialize.h"

namespace ptgn {

namespace editor {

struct EditorSettings {
	bool entity_picking{ true };
	bool gizmo_uses_local_orientation{ false };

	PTGN_SERIALIZE(EditorSettings, entity_picking, gizmo_uses_local_orientation)
};

} // namespace editor

} // namespace ptgn