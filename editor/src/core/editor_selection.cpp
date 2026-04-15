#include "core/editor_selection.h"

#include <optional>

#include "runtime/ecs/entity.h"

namespace ptgn::editor {

void EditorSelection::Clear() {
	entity = std::nullopt;
	// asset  = std::nullopt;
}

} // namespace ptgn::editor