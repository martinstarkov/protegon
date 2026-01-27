#pragma once

#include <optional>

#include "core/math/geometry/shape.h"
#include "core/math/transform.h"

namespace ptgn {

class Entity;

[[nodiscard]] Transform OffsetByOrigin(
	const Shape& shape, const Transform& transform, const Entity& entity
);

// @return The shape of the entity, if it has one.
std::optional<Shape> GetShape(const Entity& entity);

// @return The display size of the entity sprite (if it has a TextureHandle), or its shape, if it
// has one.
std::optional<Shape> GetSpriteOrShape(const Entity& entity);

} // namespace ptgn