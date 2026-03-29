#pragma once

#include <optional>
#include <ostream>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "serialization/json/enum.h"
#include "serialization/json/serialize.h"

namespace ptgn {

/// @brief Controls the lifecycle state of a behavior/component on an entity.
enum class ComponentState {
	/// @brief Component exists but is inactive.
	Disabled,
	/// @brief Component exists and is active.
	Enabled,
	/// @brief Component is removed entirely from the entity.
	Removed
};

std::ostream& operator<<(std::ostream& os, ComponentState state);

/// @brief Defines the conditions under which a drag event is triggered for a draggable or dropzone
/// entity.
enum class TriggerCondition {
	/// @brief Event is never triggered.
	None,
	/// @brief Event triggered if the mouse position overlaps the dropzone.
	MouseOverlaps,
	/// @brief Event triggered if the object's transform overlaps the dropzone.
	TransformOverlaps,
	/// @brief Event triggered if any part of the object overlaps the dropzone.
	Overlaps,
	/// @brief Event triggered if the object is entirely contained within the dropzone.
	Contains
};

std::ostream& operator<<(std::ostream& os, TriggerCondition condition);

PTGN_SERIALIZE_ENUM(
	TriggerCondition, { { TriggerCondition::None, nullptr },
						{ TriggerCondition::MouseOverlaps, "mouse_overlaps" },
						{ TriggerCondition::TransformOverlaps, "transform_overlaps" },
						{ TriggerCondition::Overlaps, "overlaps" },
						{ TriggerCondition::Contains, "contains" } }
);

/// @brief Represents the different phases of a drag event, which can be used to specify when
/// certain conditions or callbacks should be evaluated for draggable and dropzone entities.
enum class DragEventPhase {
	MoveOver,
	Drop,
	Pickup
};

std::ostream& operator<<(std::ostream& os, DragEventPhase phase);

PTGN_SERIALIZE_ENUM(
	DragEventPhase, { { DragEventPhase::MoveOver, "mouse_over" },
					  { DragEventPhase::Drop, "drop" },
					  { DragEventPhase::Pickup, "pickup" } }
);

/// @brief Add to an interactive entity to temporarily block interactions with it for the specified
/// remaining time. Automatically removed when the remaining time reaches zero.
struct InteractionLock {
	secondsf remaining_time{ 0.0f };

	bool block_hover{ true };
	bool block_press{ true };
};

namespace impl {

struct Interactive {
	Interactive()								   = default;
	~Interactive() noexcept						   = default;
	Interactive(Interactive&&) noexcept			   = default;
	Interactive& operator=(Interactive&&) noexcept = default;
	Interactive(const Interactive&)				   = delete;
	Interactive& operator=(const Interactive&)	   = delete;

	/// Interactive owns that shapes.
	/// List of entities that can be interacted with. They require a valid Rect / Circle component.
	std::vector<GameObject> shapes;

	bool enabled{ true };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Interactive, enabled)
};

struct Draggable {
	/// @brief The offset of the current mouse position compared to the mouse position where the
	/// drag started.
	V2_float offset;

	/// @brief The mouse position at which the drag started.
	V2_float start;

	/// @brief If the entity is currently being dragged.
	bool dragging{ false };

	/// @brief Dropzones the draggable is currently on.
	std::unordered_set<Entity> dropzones;

	/// @brief Dropzones the draggable was on during the previous frame (for triggering callbacks).
	std::unordered_set<Entity> last_dropzones;

	bool enabled{ true };

	TriggerCondition move_condition{ TriggerCondition::MouseOverlaps };
	TriggerCondition drop_condition{ TriggerCondition::MouseOverlaps };
	TriggerCondition pickup_condition{ TriggerCondition::Overlaps };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		Draggable, dropzones, last_dropzones, offset, start, dragging, move_condition,
		drop_condition, pickup_condition
	)
};

struct Dropzone {
	/// @brief Draggables currently on the dropzone.
	std::unordered_set<Entity> draggables;

	TriggerCondition move_condition{ TriggerCondition::MouseOverlaps };
	TriggerCondition drop_condition{ TriggerCondition::MouseOverlaps };
	TriggerCondition pickup_condition{ TriggerCondition::Overlaps };

	bool enabled{ true };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		Dropzone, draggables, move_condition, drop_condition, pickup_condition
	)
};

} // namespace impl

/// Sets the entity to be interactive, allowing it to have interactable shapes as children and
/// trigger interact scripts.
void SetInteractive(Entity entity, ComponentState state = ComponentState::Enabled);

/// @return True if the entity is interactive and enabled, false otherwise.
[[nodiscard]] bool IsInteractive(Entity entity);

/// @brief Adds an interactable shape as a child to the interactive entity.
/// @param shape The shape which must have a valid Rect or Circle component.
/// @param shape_id An optional string identifier for the shape, which can be used to reference it
/// later.
/// @param ignore_parent_transform If true, the shape's position will be treated as world space
/// instead of relative to the interactive entity's transform.
void AddInteractiveShape(
	Entity interactive_entity, GameObject&& shape, std::optional<std::string_view> shape_id = {},
	bool ignore_parent_transform = false
);

/// @brief Sets the only interactable shape of the entity to be the provided one. Will clear any
/// existing shapes.
/// @param shape The shape which must have a valid Rect or Circle component.
/// @param shape_id An optional string identifier for the shape, which can be used to reference it
/// later.
/// @param ignore_parent_transform If true, the shape's position will be treated as world space
/// instead of relative to the interactive entity's transform.
void SetInteractiveShape(
	Entity interactive_entity, GameObject&& shape, std::optional<std::string_view> shape_id = {},
	bool ignore_parent_transform = false
);

void AddInteractiveRect(
	Entity interactive_entity, V2_float position, V2_float size,
	Origin draw_origin = Origin::Center, std::optional<std::string_view> shape_id = {},
	bool ignore_parent_transform = false
);

void SetInteractiveRect(
	Entity interactive_entity, V2_float position, V2_float size,
	Origin draw_origin = Origin::Center, std::optional<std::string_view> shape_id = {},
	bool ignore_parent_transform = false
);

void AddInteractiveCircle(
	Entity interactive_entity, V2_float position, float radius,
	std::optional<std::string_view> shape_id = {}, bool ignore_parent_transform = false
);

void SetInteractiveCircle(
	Entity interactive_entity, V2_float position, float radius,
	std::optional<std::string_view> shape_id = {}, bool ignore_parent_transform = false
);

/// Remove an interactable shape from the interactive entity.
void RemoveInteractiveShape(Entity interactive_entity, std::string_view shape_id);

/// @return True if the entity has any interactable shape.
[[nodiscard]] bool HasInteractiveShape(Entity entity);

/// @return True if the entity has the given interactable.
[[nodiscard]] bool HasInteractiveShape(Entity interactive_entity, std::string_view shape_id);

/// @return Entity handles to interactable shapes attached to the entity.
std::vector<Entity> GetInteractiveShapes(Entity interactive_entity);

/// @brief Destroys all interactable shapes attached to the entity.
void ClearInteractiveShapes(Entity interactive_entity);

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

/// @brief If true, enables the entity to trigger dropzone scripts.
void SetDropzone(Entity entity, ComponentState state = ComponentState::Enabled);

/// @return True if the entity is dropzone, false otherwise.
[[nodiscard]] bool IsDropzone(Entity entity);

/// @return Dropzones that the draggable is currently dropped on.
const std::unordered_set<Entity>& GetDropzones(Entity draggable_entity);

/// @return Draggable entities which are currently dropped on the dropzone.
const std::unordered_set<Entity>& GetDraggables(Entity dropzone_entity);

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

/// @brief Assigns a condition that determines whether the specified entity should respond to drag
/// @param dropzone_entity The dropzone entity whose dropzone behavior is being configured.
/// @param phase The drag event phase (move over, drop, pickup) during which the condition will be
/// evaluated.
/// @param condition A condition that is evaluated during the specified phase; it should
/// return true to allow the drag action or false to block it.
void SetDropzoneCondition(Entity dropzone_entity, DragEventPhase phase, TriggerCondition condition);

} // namespace ptgn