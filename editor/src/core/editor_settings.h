#pragma once

#include "serialization/serialize.h"

namespace ptgn {

namespace editor {

struct EditorSettings {
	bool entity_picking{ true };

	PTGN_SERIALIZE(EditorSettings, entity_picking)
};

} // namespace editor

} // namespace ptgn