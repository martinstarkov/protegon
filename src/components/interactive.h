#pragma once

#include <unordered_set>
#include <vector>

#include "core/entity.h"
#include "math/vector2.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

class SceneInput;

// If true, enables the entity to trigger interaction scripts.
// @return entity.
Entity& SetInteractive(Entity& entity, bool interactive = true);
[[nodiscard]] bool IsInteractive(const Entity& entity);

// Add an interactable shape to the entity.
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

enum class CallbackTrigger {
	None,			// Event is never triggered.
	MouseOverlaps,	// Event triggered if the mouse position overlaps the dropzone.
	CenterOverlaps, // Event triggered if the object's center overlaps the dropzone.
	Overlaps,		// Event triggered if any part of the object overlaps the dropzone.
	Contains		// Event triggered if the object is entirely contained within the dropzone.
};

struct Draggable {
	// Offset from the drag target center. Adding this value to the target position will maintain
	// the relative position between the mouse and drag target.
	V2_float offset;
	// Mouse position where the drag started.
	V2_float start;

	bool dragging{ false };

	std::unordered_set<Entity> dropzones;

	void SetTrigger(CallbackTrigger trigger) {
		move_trigger_	= trigger;
		drop_trigger_	= trigger;
		pickup_trigger_ = trigger;
	}

	void SetMoveTrigger(CallbackTrigger trigger) {
		move_trigger_ = trigger;
	}

	void SetDropTrigger(CallbackTrigger trigger) {
		drop_trigger_ = trigger;
	}

	void SetPickupTrigger(CallbackTrigger trigger) {
		pickup_trigger_ = trigger;
	}

private:
	friend class SceneInput;

	std::unordered_set<Entity> last_dropzones_;

	CallbackTrigger move_trigger_{ CallbackTrigger::MouseOverlaps };
	CallbackTrigger drop_trigger_{ CallbackTrigger::MouseOverlaps };
	CallbackTrigger pickup_trigger_{ CallbackTrigger::Overlaps };

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		Draggable, KeyValue("dropzones", dropzones), KeyValue("last_dropzones", last_dropzones_),
		KeyValue("offset", offset), KeyValue("start", start), KeyValue("dragging", dragging),
		KeyValue("move_trigger", move_trigger_), KeyValue("drop_trigger", drop_trigger_),
		KeyValue("pickup_trigger", pickup_trigger_)
	)
};

struct Dropzone {
	void SetTrigger(CallbackTrigger trigger) {
		move_trigger_	= trigger;
		drop_trigger_	= trigger;
		pickup_trigger_ = trigger;
	}

	void SetMoveTrigger(CallbackTrigger trigger) {
		move_trigger_ = trigger;
	}

	void SetDropTrigger(CallbackTrigger trigger) {
		drop_trigger_ = trigger;
	}

	void SetPickupTrigger(CallbackTrigger trigger) {
		pickup_trigger_ = trigger;
	}

	std::unordered_set<Entity> dropped_entities;

private:
	friend class SceneInput;

	CallbackTrigger move_trigger_{ CallbackTrigger::MouseOverlaps };
	CallbackTrigger drop_trigger_{ CallbackTrigger::MouseOverlaps };
	CallbackTrigger pickup_trigger_{ CallbackTrigger::Overlaps };

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		Dropzone, KeyValue("dropped_entities", dropped_entities),
		KeyValue("move_trigger", move_trigger_), KeyValue("drop_trigger", drop_trigger_),
		KeyValue("pickup_trigger", pickup_trigger_)
	)
};

PTGN_SERIALIZER_REGISTER_ENUM(
	CallbackTrigger, { { CallbackTrigger::None, nullptr },
					   { CallbackTrigger::MouseOverlaps, "mouse_overlaps" },
					   { CallbackTrigger::CenterOverlaps, "center_overlaps" },
					   { CallbackTrigger::Overlaps, "overlaps" },
					   { CallbackTrigger::Contains, "contains" } }
);

} // namespace ptgn