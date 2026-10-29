#pragma once

#include <string>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/uuid.h"

namespace ptgn {

class Scene;

namespace editor {

class Editor;

struct EntityReference {
	std::string scene_key{};
	bool runtime{ false };
	UUID entity_uuid{};

	[[nodiscard]] Scene* ResolveScene(Editor& editor) const;
	[[nodiscard]] Entity Resolve(Editor& editor) const;

	bool operator==(const EntityReference&) const = default;
};

[[nodiscard]] EntityReference MakeEntityReference(Entity entity);

} // namespace editor

} // namespace ptgn
