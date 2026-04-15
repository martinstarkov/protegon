#pragma once

#include <vector>

#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/interaction/trigger_condition.h"
#include "serialization/serialize.h"

namespace ptgn {

namespace impl {

struct Draggable {
	/// @brief The offset of the current mouse position compared to the mouse position where the
	/// drag started.
	V2_float offset;

	/// @brief The mouse position at which the drag started.
	V2_float start;

	/// @brief If the entity is currently being dragged.
	bool dragging{ false };

	bool enabled{ true };

	TriggerCondition move_condition{ TriggerCondition::MouseOverlaps };
	TriggerCondition drop_condition{ TriggerCondition::MouseOverlaps };
	TriggerCondition pickup_condition{ TriggerCondition::Overlaps };

	/// @brief Dropzone entities that the draggable is currently dropped on.
	std::vector<Entity> dropzones;

	/// @brief Dropzones the draggable is currently hovered on.
	std::vector<Entity> hovered_dropzones;

	/// @brief Dropzones the draggable was hovered on during the previous frame (for triggering
	/// callbacks).
	std::vector<Entity> last_hovered_dropzones;

	PTGN_REFLECT(
		Draggable, offset, start, dragging, enabled, move_condition, drop_condition,
		pickup_condition
	)
};

} // namespace impl

/// @brief If true, enables the entity to trigger drag scripts.
void SetDraggable(Entity entity, ComponentState state = ComponentState::Enabled);

/// @return True if the entity is draggable and enabled, false otherwise.
[[nodiscard]] bool IsDraggable(Entity entity);

/// @return True if the entity is currently being dragged, false otherwise.
[[nodiscard]] bool IsDragging(Entity entity);

/// @return Offset from the drag target center. Adding this value to the target position will
/// maintain the relative position between the mouse and drag target.
V2_float GetDragOffset(Entity draggable_entity);

/// @return Mouse position where the drag started.
V2_float GetDragStart(Entity draggable_entity);

/// @return True if the mouse is currently dragging the draggable, false otherwise.
[[nodiscard]] bool IsBeingDragged(Entity draggable_entity);

/// @return Dropzones that the draggable is currently dropped on.
std::vector<Entity> GetDropzones(Entity draggable_entity);

/// @return Dropzones that the draggable is currently hovered over.
std::vector<Entity> GetHoveredDropzones(Entity draggable_entity);

/// @brief Assigns a condition that determines whether the specified entity should respond to drag
/// events for a given drag phase.
/// @param draggable_entity The draggable entity whose drag behavior is being configured.
/// @param phase The drag event phase (move over, drop, pickup) during which the condition
/// will be evaluated.
/// @param condition A condition that is evaluated during the specified phase; it should
/// return true to allow the drag action or false to block it.
void SetDraggableCondition(
	Entity draggable_entity, DragEventPhase phase, TriggerCondition condition
);

} // namespace ptgn