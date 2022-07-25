#pragma once

#include "math/Vector2.h"
#include "math/Math.h"

// Source: https://www.jeffreythompson.org/collision-detection/line-point.php
// Source: https://stackoverflow.com/a/7050238

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a point and a line overlap.
template <typename T>
inline bool PointvsLine(const math::Vector2<T>& point,
						const math::Vector2<T>& line_origin,
						const math::Vector2<T>& line_destination) {
	const math::Vector2<T> m{ point - line_origin };
	const V2_double direction{ line_destination - line_origin };
	const V2_double portion_to_sides{ m / direction };
	return portion_to_sides.IsEqual();
}

} // namespace overlap

} // namespace collision

} // namespace ptgn