#pragma once

#include <vector>

#include "core/entity.h"
#include "math/vector2.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

// If true, enables the entity to trigger interaction scripts.
// Note: Setting interactive to true will implicitly call Enable() on *this.
// @return entity.
Entity& SetInteractive(Entity& entity, bool interactive = true);
[[nodiscard]] bool IsInteractive(const Entity& entity);

// Add an interactable shape to the entity.
// Note: adding an interactive will implicitly call SetInteractive(true) and Enable() on *this.
// @param set_parent If true, will set the parent of shape to *this.
// The entity interactive will take ownership of these entities.
// @return entity.
Entity& AddInteractable(Entity& entity, Entity& shape, bool set_parent = true);

// Same as AddInteractable but will clear previous interactables first.
// @return entity.
Entity& SetInteractable(Entity& entity, Entity& shape, bool set_parent = true);

// Remove an interactable shape from the entity.
// @return entity.
Entity& RemoveInteractable(Entity& entity, const Entity& shape);

// @return True if the entity has the given interactable.
[[nodiscard]] bool HasInteractable(const Entity& entity, const Entity& shape);

[[nodiscard]] const std::vector<Entity>& GetInteractables(const Entity& entity);

namespace impl {

void ClearInteractables(Entity& entity);
[[nodiscard]] const Interactive& GetInteractive(const Entity& entity);
[[nodiscard]] Interactive& GetInteractive(Entity& entity);
void SetInteractiveWasInside(Entity& entity, bool value);
void SetInteractiveIsInside(Entity& entity, bool value);
[[nodiscard]] bool InteractiveWasInside(const Entity& entity);
[[nodiscard]] bool InteractiveIsInside(const Entity& entity);

} // namespace impl

struct Interactive {
	bool is_inside{ false };
	bool was_inside{ false };

	void Clear();

	// List of entities that can be interacted with. They require a valid Rect / Circle component.
	std::vector<Entity> shapes;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Interactive, is_inside, was_inside, shapes)
};

struct Draggable {
	// Offset from the drag target center. Adding this value to the target position will maintain
	// the relative position between the mouse and drag target.
	V2_float offset;
	// Mouse position where the drag started.
	V2_float start;

	bool dragging{ false };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Draggable, offset, start, dragging)
};

enum class DropTrigger {
	MouseOverlaps,	// Drop event triggered if the mouse position overlaps the dropzone.
	CenterOverlaps, // Drop event triggered if the object's center overlaps the dropzone.
	Overlaps,		// Drop event triggered if any part of the object overlaps the dropzone.
	Contains		// Drop event triggered if the object is entirely contained within the dropzone.
};

struct Dropzone {
	DropTrigger trigger{ DropTrigger::MouseOverlaps };

	std::vector<Entity> dropped_entities;

	std::vector<Entity> overlapping_draggables;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		Dropzone, trigger, dropped_entities, overlapping_draggables
	)
};

PTGN_SERIALIZER_REGISTER_ENUM(
	DropTrigger, { { DropTrigger::MouseOverlaps, "mouse_overlaps" },
				   { DropTrigger::CenterOverlaps, "center_overlaps" },
				   { DropTrigger::Overlaps, "overlaps" },
				   { DropTrigger::Contains, "contains" } }
);

} // namespace ptgn