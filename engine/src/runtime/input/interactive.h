#pragma once

#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "serialization/json/enum.h"
#include "serialization/json/serialize.h"

namespace ptgn {

/// Controls the lifecycle state of a behavior/component on an entity.
enum class ComponentState {
	Disabled, /// Component exists but is inactive.
	Enabled,  /// Component exists and is active.
	Removed	  /// Component is removed entirely from the entity.
};

enum class TriggerCondition {
	None,			   /// Event is never triggered.
	MouseOverlaps,	   /// Event triggered if the mouse position overlaps the dropzone.
	TransformOverlaps, /// Event triggered if the object's transform overlaps the dropzone.
	Overlaps,		   /// Event triggered if any part of the object overlaps the dropzone.
	Contains		   /// Event triggered if the object is entirely contained within the dropzone.
};

PTGN_SERIALIZE_ENUM(
	TriggerCondition, { { TriggerCondition::None, nullptr },
						{ TriggerCondition::MouseOverlaps, "mouse_overlaps" },
						{ TriggerCondition::TransformOverlaps, "transform_overlaps" },
						{ TriggerCondition::Overlaps, "overlaps" },
						{ TriggerCondition::Contains, "contains" } }
);

enum class DragEventPhase {
	MoveOver,
	Drop,
	Pickup
};

PTGN_SERIALIZE_ENUM(
	DragEventPhase, { { DragEventPhase::MoveOver, "mouse_over" },
					  { DragEventPhase::Drop, "drop" },
					  { DragEventPhase::Pickup, "pickup" } }
);

namespace impl {

struct Interactive {
	Interactive()								   = default;
	~Interactive() noexcept						   = default;
	Interactive(Interactive&&) noexcept			   = default;
	Interactive& operator=(Interactive&&) noexcept = default;
	Interactive(const Interactive&)				   = delete;
	Interactive& operator=(const Interactive&)	   = delete;

	/// Destroys all the shape entities and clears the shapes vector.
	void ClearShapes();

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

/// If true, enables the entity to trigger interaction scripts.
/// @return entity.
Entity SetInteractive(Entity entity, bool interactive = true);

/// Removes an entity's interactive component entirely.
Entity RemoveInteractive(Entity entity);

[[nodiscard]] bool IsInteractive(Entity entity);

/// Add an interactable shape to the entity.
/// @param set_parent If true, will set the parent of shape to *this.
/// The entity interactive will take ownership of these entities.
/// @return entity.
Entity AddInteractable(
	Entity entity, GameObject&& shape, std::optional<std::string_view> name = {},
	bool ignore_parent_transform = false
);

/// Same as AddInteractable but will clear previous interactables first.
/// @return entity.
Entity SetInteractable(
	Entity entity, GameObject&& shape, std::optional<std::string_view> name = {},
	bool ignore_parent_transform = false
);

/// Remove an interactable shape from the entity.
/// @return entity.
Entity RemoveInteractable(Entity entity, std::string_view name);

/// @return True if the entity has the given interactable.
[[nodiscard]] bool HasInteractable(Entity entity, std::string_view name);

[[nodiscard]] std::vector<Entity> GetInteractables(Entity entity);

void ClearInteractables(Entity entity);

/// @brief If true, enables the entity to trigger drag scripts.
/// @return entity.
Entity SetDraggable(Entity entity, bool draggable = true);

/// @brief Removes an entity's draggable component entirely.
/// @return entity.
Entity RemoveDraggable(Entity entity);

/// @return True if the entity is draggable, false otherwise.
[[nodiscard]] bool IsDraggable(Entity entity);

/// @return Offset from the drag target center. Adding this value to the target position will
/// maintain the relative position between the mouse and drag target.
[[nodiscard]] V2_float GetDragOffset(Entity draggable);

/// @return Mouse position where the drag started.
[[nodiscard]] V2_float GetDragStart(Entity draggable);

/// @return True if the mouse is currently dragging the draggable, false otherwise.
[[nodiscard]] bool IsBeingDragged(Entity draggable);

/// @brief If true, enables the entity to trigger dropzone scripts.
/// @return entity.
Entity SetDropzone(Entity entity, bool dropzone = true);

/// @brief Removes an entity's dropzone component entirely.
/// @return entity.
Entity RemoveDropzone(Entity entity);

/// @return True if the entity is dropzone, false otherwise.
[[nodiscard]] bool IsDropzone(Entity entity);

/// @return Dropzones that the draggable is currently dropped on.
[[nodiscard]] const std::unordered_set<Entity>& GetDropzones(Entity draggable);

/// @return Draggable entities which are currently dropped on the dropzone.
[[nodiscard]] const std::unordered_set<Entity>& GetDraggables(Entity dropzone);

void SetDraggableCondition(Entity draggable, DragEventPhase phase, TriggerCondition condition);

void SetDropzoneCondition(Entity dropzone, DragEventPhase phase, TriggerCondition condition);

} // namespace ptgn