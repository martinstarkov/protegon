#pragma once

#include <optional>
#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "serialization/json/json.h"

namespace ptgn {

class Scene;

namespace editor {

struct EntitySnapshot {
	UUID uuid;
	Tag tag;
	json components = json::object();
	std::optional<UUID> parent_uuid;
	std::vector<EntitySnapshot> children;
};

[[nodiscard]] EntitySnapshot CaptureEntitySnapshot(Entity entity);
[[nodiscard]] Entity RestoreEntitySnapshot(Scene& scene, const EntitySnapshot& snapshot);
void DestroyEntitySnapshot(Scene& scene, const EntitySnapshot& snapshot);

} // namespace editor

} // namespace ptgn
