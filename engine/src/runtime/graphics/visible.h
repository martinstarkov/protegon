#pragma once

#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

struct IgnoreParentVisibility {};

struct Visible {
	bool visible{ false };
};

} // namespace impl

namespace event {

/// @brief This event is emitted when an entity becomes visible, either through Show() or
/// SetVisible(true).
struct EntityShow {
	operator Entity() const { // NOSONAR
		return entity;
	}

	Entity entity;
};

/// @brief This event is emitted when an entity becomes hidden, either through Hide() or
/// SetVisible(false).
struct EntityHide {
	operator Entity() const { // NOSONAR
		return entity;
	}

	Entity entity;
};

} // namespace event

void SetVisible(Entity entity, bool visible, bool emit_visibility_event = true);

void Show(Entity entity, bool emit_visibility_event = true);

void Hide(Entity entity, bool emit_visibility_event = true);

/// @param check_parent_visibility If true, the visibility of parent entities will be checked. If
/// any parent is hidden, this entity will be considered hidden as well.
/// @return True if the entity is visible, false otherwise.
bool IsVisible(Entity entity, bool check_parent_visibility = true);

/// @brief Ignores the visibility of parent entities when determining if this entity is visible.
void IgnoreParentVisibility(Entity entity, bool ignore = true);

} // namespace ptgn