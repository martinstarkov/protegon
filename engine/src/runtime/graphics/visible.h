#pragma once

#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

struct Visible {};

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

/// @return True if the entity is visible, false otherwise.
[[nodiscard]] bool IsVisible(Entity entity);

} // namespace ptgn