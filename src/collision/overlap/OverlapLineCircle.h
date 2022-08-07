#pragma once

#include <type_traits> // std::enable_if_t, ...

#include "collision/overlap/OverlapPointCircle.h"
#include "math/Vector2.h"
#include "math/Math.h"

// Source: https://www.jeffreythompson.org/collision-detection/line-circle.php
// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 179.
// Source: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
// Source (used): https://www.baeldung.com/cs/circle-line-segment-collision-detection

namespace ptgn {

namespace math {

// Get the area of the triangle formed by points A, B, C.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
inline S TriangleArea(const math::Vector2<T>& A,
					  const math::Vector2<T>& B,
					  const math::Vector2<T>& C) {
	const math::Vector2<S> AB{ B - A };
	const math::Vector2<S> AC{ C - A };
	return math::FastAbs(AB.Cross(AC)) / 2;
}

} // namespace math

namespace collision {

namespace overlap {

// Check if a line and a circle overlap.
// Circle position is taken from its center.
template <typename T, typename S = double,
	std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
static bool LinevsCircle(const math::Vector2<T>& line_origin,
					     const math::Vector2<T>& line_destination,
					     const math::Vector2<T>& circle_position,
					     const T circle_radius) {
	static_assert(!tt::is_narrowing_v<T, S>);
	// If the line is inside the circle entirely, exit early.
	if (PointvsCircle(line_origin, circle_position, circle_radius) && PointvsCircle(line_destination, circle_position, circle_radius)) return true;
	S minimum_distance_squared{ std::numeric_limits<S>::infinity() };
	const S radius_squared{ static_cast<S>(circle_radius) * static_cast<S>(circle_radius) };
	// O is the circle center, P is the line origin, Q is the line destination.
	const math::Vector2<S> OP{ line_origin - circle_position };
	const math::Vector2<S> OQ{ line_destination - circle_position };
	const math::Vector2<S> PQ{ line_destination - line_origin };
	const S OP_distance_squared{ OP.MagnitudeSquared() };
	const S OQ_distance_squared{ OQ.MagnitudeSquared() };
	const S maximum_distance_squared{ std::max(OP_distance_squared, OQ_distance_squared) };
	if (OP.Dot(-PQ) > 0 && OQ.Dot(PQ) > 0) {
		const S triangle_area{ math::TriangleArea<S>(circle_position, line_origin, line_destination) };
		minimum_distance_squared = 4 * triangle_area * triangle_area / PQ.MagnitudeSquared();
	} else {
		minimum_distance_squared = std::min(OP_distance_squared, OQ_distance_squared);
	}
	return (minimum_distance_squared < radius_squared || math::Compare(minimum_distance_squared, radius_squared)) && 
		   (maximum_distance_squared > radius_squared || math::Compare(maximum_distance_squared, radius_squared));
}

} // namespace overlap

} // namespace collision

} // namespace ptgn