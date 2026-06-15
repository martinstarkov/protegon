#pragma once

#include <span>

#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

class ColliderShape;

struct BoundingAABB {
	V2_float min;
	V2_float max;

	[[nodiscard]] bool Overlaps(BoundingAABB other) const;

	[[nodiscard]] bool Overlaps(V2_float point) const;

	[[nodiscard]] BoundingAABB ExpandByVelocity(V2_float velocity) const;
};

/// @return Axis aligned bounding box which contains the given vertices (fully surrounding them).
BoundingAABB GetBoundingAABB(std::span<const V2_float> vertices);

/// @return Axis aligned bounding box which contains the given shape (fully surrounding it).
template <ColliderType T>
BoundingAABB GetBoundingAABB(const T& shape, Transform transform) {
	auto world_vertices{ GetWorldVertices(shape, transform) };

	return GetBoundingAABB(world_vertices);
}

BoundingAABB GetBoundingAABB(const ColliderShape& shape, Transform transform);

} // namespace ptgn