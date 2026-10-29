#include "runtime/physics/bounding_aabb.h"

#include <span>

#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

bool BoundingAABB::Overlaps(BoundingAABB other) const {
	return !(
		max.x < other.min.x || min.x > other.max.x || max.y < other.min.y || min.y > other.max.y
	);
}

bool BoundingAABB::Overlaps(V2_float point) const {
	return !(max.x < point.x || min.x > point.x || max.y < point.y || min.y > point.y);
}

BoundingAABB BoundingAABB::ExpandByVelocity(V2_float velocity) const {
	if (velocity.IsZero()) {
		return *this;
	}

	BoundingAABB expanded{ *this };

	if (velocity.x > 0) {
		expanded.max.x += velocity.x;
	} else {
		expanded.min.x += velocity.x;
	}

	if (velocity.y > 0) {
		expanded.max.y += velocity.y;
	} else {
		expanded.min.y += velocity.y;
	}

	return expanded;
}

BoundingAABB GetBoundingAABB(std::span<const V2_float> vertices) {
	auto min{ vertices.front() };
	auto max{ vertices.front() };

	for (const auto& v : vertices.subspan(1)) {
		min = Min(min, v);
		max = Max(max, v);
	}

	return BoundingAABB{ min, max };
}

BoundingAABB GetBoundingAABB(const ColliderShape& shape, Transform transform) {
	return shape.Visit([transform](const auto& s) { return GetBoundingAABB(s, transform); });
}

} // namespace ptgn