#pragma once

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

/// @return Axis aligned bounding box which contains the given shape (fully surrounding it).
BoundingAABB GetBoundingAABB(const ColliderShape& shape, Transform transform);

} // namespace ptgn