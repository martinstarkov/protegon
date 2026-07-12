#pragma once

#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/interaction/trigger_condition.h"
#include "serialization/serialize.h"

namespace ptgn {

namespace impl {

struct Dropzone {
	TriggerCondition move_condition{ TriggerCondition::MouseOverlaps };
	TriggerCondition drop_condition{ TriggerCondition::MouseOverlaps };
	TriggerCondition pickup_condition{ TriggerCondition::Overlaps };

	bool enabled{ true };

	/// @brief Draggables currently on the dropzone.
	std::vector<Entity> draggables;

	PTGN_REFLECT(Dropzone, move_condition, drop_condition, pickup_condition, enabled)
};

} // namespace impl

/// @brief If true, enables the entity to trigger dropzone scripts.
void SetDropzone(Entity entity, ComponentState state = ComponentState::Enabled);

/// @return True if the entity is dropzone, false otherwise.
[[nodiscard]] bool IsDropzone(Entity entity);

/// @return Draggable entities which are currently dropped on the dropzone.
std::vector<Entity> GetDraggables(Entity dropzone_entity);

/// @brief Assigns a condition that determines whether the specified entity should respond to drag
/// @param dropzone_entity The dropzone entity whose dropzone behavior is being configured.
/// @param phase The drag event phase (move over, drop, pickup) during which the condition will be
/// evaluated.
/// @param condition A condition that is evaluated during the specified phase; it should
/// return true to allow the drag action or false to block it.
void SetDropzoneCondition(Entity dropzone_entity, DragEventPhase phase, TriggerCondition condition);

} // namespace ptgn