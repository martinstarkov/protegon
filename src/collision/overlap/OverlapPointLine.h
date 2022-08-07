#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Source: https://www.jeffreythompson.org/collision-detection/line-point.php
// Source (used): https://stackoverflow.com/a/7050238
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
	inline bool PointLine(const math::Vector2<T>& point,
						  const math::Vector2<T>& line_origin,
						  const math::Vector2<T>& line_destination) {
	const math::Vector2<S> point_to_origin{ point - line_origin };
	const math::Vector2<S> direction{ line_destination - line_origin };
	const math::Vector2<S> gradient{ point_to_origin / direction };
	// Check that the gradient is the same along both axes, i.e. "colinear".
	const math::Vector2<S> min{ math::Min(line_origin, line_destination) };
	const math::Vector2<S> max{ math::Max(line_origin, line_destination) };
	// Edge cases where line aligns with an axis.
	// TODO: Check that this is correct.
	if (math::Compare(direction.x, 0) && math::Compare(point.x, line_origin.x)) {
		if (point.y < min.y || point.y > max.y) return false;
		return true;
	}
	if (math::Compare(direction.y, 0) && math::Compare(point.y, line_origin.y)) {
		if (point.x < min.x || point.x > max.x) return false;
		return true;
	}
	return gradient.IsEqual() && PointvsAABB(static_cast<math::Vector2<S>>(point), min, max - min);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn
