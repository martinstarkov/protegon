#pragma once

#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"

namespace ptgn {

namespace impl {

struct IgnoreParentTint {};

struct Tint {
	Color value{ color::White };

	operator Color() const { // NOSONAR
		return value;
	}

	PTGN_SERIALIZE_VALUE(Tint, value)
};

} // namespace impl

/// @param color color::White will clear any tint.
void SetTint(Entity entity, Color color = color::White);

Color GetTint(Entity entity);

void IgnoreParentTint(Entity entity, bool ignore_parent_tint = true);

} // namespace ptgn