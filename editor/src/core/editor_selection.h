#pragma once

#include <optional>

#include "runtime/ecs/entity.h"

namespace ptgn::editor {

struct EditorSelection {
	std::optional<Entity> entity;

	// AssetHandle asset;

	void Clear();
};

} // namespace ptgn::editor