#pragma once

#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"

namespace ptgn {

namespace impl {

struct IgnoreParentTint {};

} // namespace impl

struct Tint {
	Color value{ color::White };

	operator Color() const { // NOSONAR
		return value;
	}

	PTGN_REFLECT_VALUE(Tint, value)
};

/// @return Tint of the entity including all of its parent tints.
Color GetTint(Entity entity);

/// @brief Sets the tint of the entity. Setting to white will clear any tint.
void SetTint(Entity entity, Color tint = color::White);

/// @brief If true, make it so the entity ignores its parent tint(s).
void IgnoreParentTint(Entity entity, bool ignore_parent_tint = true);

} // namespace ptgn