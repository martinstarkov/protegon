#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/interaction/trigger_condition.h"
#include "serialization/serialize.h"

namespace ptgn {

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
	std::vector<GameObject<>> shapes;

	bool enabled{ true };

	PTGN_REFLECT(Interactive, enabled)
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
	Entity interactive_entity, GameObject<>&& shape, std::optional<std::string_view> shape_id = {},
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
	Entity interactive_entity, GameObject<>&& shape, std::optional<std::string_view> shape_id = {},
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

} // namespace ptgn