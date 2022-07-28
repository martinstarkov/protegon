#pragma once

#include <type_traits> // std::enable_if_t, ...

#include "math/Vector2.h"
#include "collision/overlap/OverlapPointAABB.h"

// Source: https://www.jeffreythompson.org/collision-detection/line-point.php
// Source (used): https://stackoverflow.com/a/7050238

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a point and a line overlap.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static bool PointvsLine(const math::Vector2<T>& point,
						const math::Vector2<T>& line_origin,
						const math::Vector2<T>& line_destination) {
	const math::Vector2<S> point_to_origin{ point - line_origin };
	const math::Vector2<S> direction{ line_destination - line_origin };
	const math::Vector2<S> gradient{ point_to_origin / direction };
	// Check that the gradient is the same along both axes, i.e. "colinear".
	const math::Vector2<S> min{ math::Min(line_origin, line_destination) };
	const math::Vector2<S> max{ math::Max(line_origin, line_destination) };
	return gradient.IsEqual() && PointvsAABB(static_cast<math::Vector2<S>>(point), min, max - min);
}

} // namespace overlap

} // namespace collision

} // namespace ptgn