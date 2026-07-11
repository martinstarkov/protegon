#pragma once

#include "serialization/serialize.h"

namespace ptgn {

namespace editor {

enum class EditorEntityPickingMode : std::uint8_t {
	Automatic,
	Enabled,
	Disabled
};
PTGN_SERIALIZE_ENUM(EditorEntityPickingMode)

struct EditorSettings {
	EditorEntityPickingMode entity_picking{ EditorEntityPickingMode::Automatic };

	PTGN_SERIALIZE(EditorSettings, entity_picking)
};

} // namespace editor

} // namespace ptgn