#pragma once

#include "math/geometry.h"
#include "math/vector2.h"

namespace ptgn {

struct Transform;

struct BoundingAABB {
	V2_float min;
	V2_float max;

	[[nodiscard]] bool Overlaps(const BoundingAABB& other) const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;

	[[nodiscard]] BoundingAABB ExpandByVelocity(const V2_float& velocity) const;
};

// @return Axis aligned bounding box which contains the given shape (fully surrounding it).
[[nodiscard]] BoundingAABB GetBoundingAABB(const Shape& shape, const Transform& transform);

} // namespace ptgn