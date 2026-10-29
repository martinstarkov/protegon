#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/interaction/trigger_condition.h"
#include "serialization/serialize.h"

namespace ptgn {

/// @brief Add to an interactive entity to temporarily block interactions with it for the specified
/// remaining time. Automatically removed when the remaining time reaches zero.
struct InteractionLock {
	bool block_hover{ true };
	bool block_press{ true };

	secondsf remaining_time{ 0.0f };

	PTGN_REFLECT_READONLY(InteractionLock, block_hover, block_press, remaining_time)
};

namespace impl {

struct InteractiveTag {};

struct Interactive {
	bool enabled{ true };

	PTGN_REFLECT_VALUE(Interactive, enabled)
};

} // namespace impl

/// Sets the entity to be interactive, allowing it to have interactable shapes as children and
/// trigger interact scripts.
void SetInteractive(Entity entity, bool enabled = true);

/// @return True if the entity is interactive and enabled, false otherwise.
[[nodiscard]] bool IsInteractive(Entity entity);

/// @brief Adds an interactable shape as a child to the interactive entity.
/// @param shape The shape which must have a valid Rect or Circle component.
/// @param shape_id An optional string identifier for the shape, which can be used to reference it
/// later.
/// @param ignore_parent_transform If true, the shape's position will be treated as world space
/// instead of relative to the interactive entity's transform.
void AddInteractiveShape(
	Entity interactive_entity, Entity shape, std::optional<std::string_view> shape_id = std::nullopt,
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
	Entity interactive_entity, Entity shape, std::optional<std::string_view> shape_id = std::nullopt,
	bool ignore_parent_transform = false
);

void AddInteractiveRect(
	Entity interactive_entity, Transform transform, V2_float size,
	Origin origin = Origin::Center, std::optional<std::string_view> shape_id = std::nullopt,
	bool ignore_parent_transform = false
);

void SetInteractiveRect(
	Entity interactive_entity, Transform transform, V2_float size,
	Origin origin = Origin::Center, std::optional<std::string_view> shape_id = std::nullopt,
	bool ignore_parent_transform = false
);

void AddInteractiveCircle(
	Entity interactive_entity, Transform transform, float radius,
	std::optional<std::string_view> shape_id = std::nullopt, bool ignore_parent_transform = false
);

void SetInteractiveCircle(
	Entity interactive_entity, Transform transform, float radius,
	std::optional<std::string_view> shape_id = std::nullopt, bool ignore_parent_transform = false
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