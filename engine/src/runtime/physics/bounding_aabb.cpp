#include "runtime/physics/bounding_aabb.h"

#include <algorithm>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
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

BoundingAABB GetBoundingAABB(const ColliderShape& shape, Transform transform) {
	auto world_vertices{ GetWorldVertices(shape, transform) };

	V2_float min{ world_vertices[0] };
	V2_float max{ world_vertices[0] };

	for (const auto& v : world_vertices) {
		min.x = std::min(min.x, v.x);
		min.y = std::min(min.y, v.y);
		max.x = std::max(max.x, v.x);
		max.y = std::max(max.y, v.y);
	}

	return BoundingAABB{ min, max };
}

} // namespace ptgn