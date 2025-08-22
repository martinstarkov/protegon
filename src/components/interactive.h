#pragma once

#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/entity.h"
#include "core/game_object.h"
#include "math/vector2.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

class SceneInput;

// If true, enables the entity to trigger interaction scripts.
// @return entity.
Entity& SetInteractive(Entity& entity, bool interactive = true);

// Removes an entity's interactive component entirely.
Entity& RemoveInteractive(Entity& entity);

[[nodiscard]] bool IsInteractive(const Entity& entity);

// Add an interactable shape to the entity.
// @param set_parent If true, will set the parent of shape to *this.
// The entity interactive will take ownership of these entities.
// @return entity.
Entity& AddInteractable(
	Entity& entity, Entity&& shape, std::string_view name = {}, bool ignore_parent_transform = false
);

// Same as AddInteractable but will clear previous interactables first.
// @return entity.
Entity& SetInteractable(
	Entity& entity, Entity&& shape, std::string_view name = {}, bool ignore_parent_transform = false
);

// Remove an interactable shape from the entity.
// @return entity.
Entity& RemoveInteractable(Entity& entity, std::string_view name);

// @return True if the entity has the given interactable.
[[nodiscard]] bool HasInteractable(const Entity& entity, std::string_view name);

[[nodiscard]] std::vector<Entity> GetInteractables(const Entity& entity);

namespace impl {

[[nodiscard]] const Interactive& GetInteractive(const Entity& entity);

[[nodiscard]] Interactive& GetInteractive(Entity& entity);

void ClearInteractables(Entity& entity);

} // namespace impl

struct Interactive {
	Interactive()								   = default;
	~Interactive()								   = default;
	Interactive(Interactive&&) noexcept			   = default;
	Interactive& operator=(Interactive&&) noexcept = default;
	Interactive(const Interactive&)				   = delete;
	Interactive& operator=(const Interactive&)	   = delete;

	// Destroys all the shape entities and clears the shapes vector.
	void ClearShapes();

	// Interactive owns that shapes.
	// List of entities that can be interacted with. They require a valid Rect / Circle component.
	std::vector<GameObject<>> shapes;

	bool enabled{ true };

	friend void to_json(nlohmann::json& nlohmann_json_j, const Interactive& nlohmann_json_t) {
		if constexpr (std::is_default_constructible_v<Interactive>) {
			const Interactive nlohmann_json_default_obj{};
			(void)nlohmann_json_default_obj;
			if constexpr (ptgn::impl::has_equality_v<
							  std::remove_reference_t<decltype(nlohmann_json_t.shapes)>,
							  std::remove_reference_t<decltype(nlohmann_json_default_obj.shapes
							  )>>) {
				if (!ptgn::impl::CompareValues(
						nlohmann_json_t.shapes, nlohmann_json_default_obj.shapes
					)) {
					nlohmann_json_j["shapes"] = nlohmann_json_t.shapes;
				}
			} else {
				nlohmann_json_j["shapes"] = nlohmann_json_t.shapes;
			}
		} else {
			nlohmann_json_j["shapes"] = nlohmann_json_t.shapes;
		}
	}

	friend void from_json(const nlohmann::json& nlohmann_json_j, Interactive& nlohmann_json_t) {
		Interactive nlohmann_json_default_obj{};
		if (auto nlohmann_json_j_value{
				nlohmann_json_j.contains("shapes") ? nlohmann_json_j.at("shapes") : json{} };
			nlohmann_json_j_value.empty()) {
			nlohmann_json_t.shapes = std::move(nlohmann_json_default_obj.shapes);
		} else {
			nlohmann_json_t.shapes =
				nlohmann_json_j.value("shapes", std::move(nlohmann_json_default_obj.shapes));
		}
	}

	friend std::ostream& operator<<(std::ostream& os, const Interactive& p) {
		json j = p;
		os << j.dump(4);
		return os;
	}
};

enum class CallbackTrigger {
	None,			// Event is never triggered.
	MouseOverlaps,	// Event triggered if the mouse position overlaps the dropzone.
	CenterOverlaps, // Event triggered if the object's center overlaps the dropzone.
	Overlaps,		// Event triggered if any part of the object overlaps the dropzone.
	Contains		// Event triggered if the object is entirely contained within the dropzone.
};

struct Draggable {
	// @return Offset from the drag target center. Adding this value to the target position will
	// maintain the relative position between the mouse and drag target.
	[[nodiscard]] V2_float GetOffset() const;

	// @return Mouse position where the drag started.
	[[nodiscard]] V2_float GetStart() const;

	// @return Dropzones that the draggable is currently dropped on.
	[[nodiscard]] const std::unordered_set<Entity>& GetDropzones() const;

	// @return True if the mouse is currently dragging the draggable, false otherwise.
	[[nodiscard]] bool IsBeingDragged() const;

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

	V2_float offset_;

	V2_float start_;

	bool dragging_{ false };

	std::unordered_set<Entity> dropzones_;

	std::unordered_set<Entity> last_dropzones_;

	CallbackTrigger move_trigger_{ CallbackTrigger::MouseOverlaps };
	CallbackTrigger drop_trigger_{ CallbackTrigger::MouseOverlaps };
	CallbackTrigger pickup_trigger_{ CallbackTrigger::Overlaps };

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		Draggable, KeyValue("dropzones", dropzones_), KeyValue("last_dropzones", last_dropzones_),
		KeyValue("offset", offset_), KeyValue("start", start_), KeyValue("dragging", dragging_),
		KeyValue("move_trigger", move_trigger_), KeyValue("drop_trigger", drop_trigger_),
		KeyValue("pickup_trigger", pickup_trigger_)
	)
};

struct Dropzone {
	void SetTrigger(CallbackTrigger trigger);

	void SetMoveTrigger(CallbackTrigger trigger);

	void SetDropTrigger(CallbackTrigger trigger);

	void SetPickupTrigger(CallbackTrigger trigger);

	// @return Entities which are currently dropped on the dropzone
	[[nodiscard]] const std::unordered_set<Entity>& GetDroppedEntities() const;

private:
	friend class SceneInput;

	std::unordered_set<Entity> dropped_entities_;

	CallbackTrigger move_trigger_{ CallbackTrigger::MouseOverlaps };
	CallbackTrigger drop_trigger_{ CallbackTrigger::MouseOverlaps };
	CallbackTrigger pickup_trigger_{ CallbackTrigger::Overlaps };

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		Dropzone, KeyValue("dropped_entities", dropped_entities_),
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