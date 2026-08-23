#pragma once

#include <optional>

#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/uuid.h"

namespace ptgn {

class Scene;

namespace editor {

/// @brief Serializable entity subtree plus editor restoration context.
///
/// root contains the complete entity hierarchy. parent_uuid records the
/// parent outside that hierarchy so deleting and undoing a child restores
/// it to its original owner.
struct EntitySnapshot {
	SerializedEntity root{};
	std::optional<UUID> parent_uuid{};
};

[[nodiscard]] EntitySnapshot CaptureEntitySnapshot(
	Entity entity
);

Entity RestoreEntitySnapshot(
	Scene& scene,
	const EntitySnapshot& snapshot
);

void DestroyEntitySnapshot(
	Scene& scene,
	const EntitySnapshot& snapshot
);

} // namespace editor

} // namespace ptgn