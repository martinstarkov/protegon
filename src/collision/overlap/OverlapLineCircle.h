#pragma once

#include "math/Vector2.h"

namespace ptgn {

namespace collision {

namespace overlap {

// Check if a line and a circle overlap.
// Circle position is taken from its center.
template <typename T>
inline bool LinevsCircle(const math::Vector2<T>& line_origin,
					     const math::Vector2<T>& line_destination,
					     const math::Vector2<T>& circle_position,
					     const T circle_radius) {
	math::Vector2<T> m{ line_origin - circle_position };
	T c{ m.DotProduct(m) - circle_radius * circle_radius };
	// If there is definitely at least one real root, there must be an intersection
	// TODO: Check if this should be an epsilon comparison for floating points.
	if (c <= T(0)) return true;
	T b{ m.DotProduct(line_destination - line_origin) };
	// Early exit if ray origin outside sphere and ray pointing away from sphere
	if (b > T(0)) return false;
	T disc{ b * b - c };
	// A negative discriminant corresponds to ray missing sphere
	if (disc < T(0)) return false;
	// Now ray must hit sphere
	return true;
}

} // namespace overlap

} // namespace collision

} // namespace ptgn